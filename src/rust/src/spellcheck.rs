// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Spell-checking engine.
//!
//! Pure-Rust, built on [`spellbook`] (which reads the Hunspell `.dic`/`.aff`
//! dictionary format) — no system spell-check library is linked. Dictionaries
//! are discovered from three places:
//!
//!  1. a bundled `en_US` copy, registered from C++ at startup out of the Qt
//!     resource system (so a fresh install always has working English);
//!  2. the per-user override directory `<AppData>/komai/hunspell/`;
//!  3. the standard system Hunspell locations (`/usr/share/hunspell`,
//!     `/usr/share/myspell/dicts`, `$XDG_DATA_DIRS/hunspell`, …) — and, inside
//!     a Flatpak, the `org.freedesktop.Platform.Hunspell.*` runtime extensions
//!     mounted at `/usr/share/hunspell`.
//!
//! The C++ side owns the `QSyntaxHighlighter` that draws the squiggles and the
//! settings UI; this module owns dictionary discovery + the lazily-parsed
//! dictionary cache + tokenisation + the misspelling check + correction
//! suggestions + the personal/ignore word lists. All of that is reached
//! through the small CXX surface re-exported by [`crate::ffi`].
//!
//! **Multi-language model.** Several dictionaries can be enabled at once. A
//! word is reported misspelled only when *none* of the enabled dictionaries
//! whose script matches the word's script accepts it. For a word in a script
//! none of the enabled dictionaries cover, we don't flag it at all (we have no
//! opinion). Correction suggestions are gathered per script-compatible
//! dictionary and returned grouped by language so the UI can label them.

use std::collections::{HashMap, HashSet};
use std::path::{Path, PathBuf};
use std::sync::{Mutex, OnceLock};

use linkify::{LinkFinder, LinkKind};
use spellbook::Dictionary;
use unicode_script::{Script, UnicodeScript};

use crate::ffi::{
    SpellcheckBlockResult, SpellcheckDictionaryEntry, SpellcheckRange, SpellcheckSuggestionGroup,
};

// ---------------------------------------------------------------------------
// Limits — keep a single oversized paste from melting a UI thread.
// ---------------------------------------------------------------------------

/// Blocks longer than this are not checked at all (a code dump pasted into the
/// composer, say). One QTextDocument block is normally a single line.
const MAX_BLOCK_CHARS: usize = 20_000;
/// Tokens longer than this are skipped (almost certainly not a real word).
const MAX_WORD_CHARS: usize = 80;
/// Cap on misspelled ranges returned for one block.
const MAX_RANGES_PER_BLOCK: usize = 2_000;
/// Cap on the number of distinct dictionaries we will ever hold parsed.
const MAX_LOADED_DICTIONARIES: usize = 16;

// The CXX-facing data types (`SpellcheckDictionaryEntry`, `SpellcheckRange`,
// `SpellcheckBlockResult`, `SpellcheckSuggestionGroup`) are declared in the
// `#[cxx::bridge]` block in `crate::ffi` and imported above.

// ---------------------------------------------------------------------------
// Global engine state
// ---------------------------------------------------------------------------

struct BuiltinDictionarySource {
    code: String,
    aff: String,
    dic: String,
}

#[derive(Default)]
struct SpellcheckState {
    /// Master on/off (the "Enable spell checking" setting). When off, nothing
    /// is ever flagged.
    master_enabled: bool,
    /// Source text of the bundled dictionary (registered once from C++).
    builtin: Option<BuiltinDictionarySource>,
    /// Discovered dictionaries (built-in first, then disk), deduplicated by
    /// canonical path. Recomputed by `discover()`.
    discovered: Vec<DiscoveredDictionary>,
    /// Locale codes the user has enabled (subset of `discovered` codes).
    enabled_codes: Vec<String>,
    /// Parsed dictionaries, keyed by locale code. Never evicted on a room/tab
    /// switch — only when a language is disabled (and even then we keep a small
    /// grace cache so a quick toggle-off/on doesn't re-parse).
    loaded: HashMap<String, LoadedDictionary>,
    /// Profile/app data directory; `personal.dic` lives directly under it and
    /// `hunspell/` under it is one of the dictionary search dirs.
    data_dir: PathBuf,
    /// Words the user added permanently, lower-cased for case-insensitive match.
    personal: HashSet<String>,
}

struct DiscoveredDictionary {
    code: String,
    /// `.dic` path; `None` for the built-in.
    path: Option<PathBuf>,
    builtin: bool,
}

struct LoadedDictionary {
    /// `None` if the dictionary failed to parse (we remember the failure so we
    /// don't retry it on every keystroke).
    dict: Option<Dictionary>,
    /// Scripts this dictionary is responsible for, derived from its locale.
    /// Empty means "unknown — applies to any script".
    scripts: Vec<Script>,
}

fn state() -> &'static Mutex<SpellcheckState> {
    static STATE: OnceLock<Mutex<SpellcheckState>> = OnceLock::new();
    STATE.get_or_init(|| Mutex::new(SpellcheckState::default()))
}

// ---------------------------------------------------------------------------
// Public (CXX) entry points
// ---------------------------------------------------------------------------

/// Register the dictionary bundled in the Qt resource system. Called once,
/// early, with the `.aff` and `.dic` file contents. Idempotent.
pub(crate) fn spellcheck_register_builtin_dictionary(code: &str, aff: &str, dic: &str) {
    if code.is_empty() || aff.is_empty() || dic.is_empty() {
        return;
    }
    let mut st = lock();
    st.builtin = Some(BuiltinDictionarySource {
        code: normalize_code(code),
        aff: aff.to_owned(),
        dic: dic.to_owned(),
    });
}

/// Re-scan the standard locations and return everything we found (built-in
/// first), so the settings UI can present the toggle list. C++ maps the codes
/// to display names and sorts them.
pub(crate) fn spellcheck_discover_dictionaries() -> Vec<SpellcheckDictionaryEntry> {
    let mut st = lock();
    discover(&mut st);
    st.discovered
        .iter()
        .map(|d| SpellcheckDictionaryEntry {
            code: d.code.clone(),
            path: d
                .path
                .as_ref()
                .map(|p| p.to_string_lossy().into_owned())
                .unwrap_or_default(),
            builtin: d.builtin,
        })
        .collect()
}

/// Apply the user's configuration: the master on/off flag, the list of enabled
/// locale codes (resolved against the discovered dictionaries), and the
/// profile/app data directory (`personal.dic` lives directly under it, and
/// `hunspell/` under it is searched for extra dictionaries). Dictionaries are
/// parsed lazily on first use, not here; ones no longer enabled are dropped
/// once the cache grows large.
pub(crate) fn spellcheck_set_config(data_dir: &str, master_enabled: bool, enabled_codes: &Vec<String>) {
    let mut st = lock();
    st.master_enabled = master_enabled;
    // Make sure discovery has run at least once so `enabled_codes` can be
    // resolved against real dictionaries later.
    if st.discovered.is_empty() {
        discover(&mut st);
    }
    let mut seen = HashSet::new();
    st.enabled_codes = enabled_codes
        .iter()
        .map(|c| normalize_code(c))
        .filter(|c| !c.is_empty() && seen.insert(c.clone()))
        .collect();

    // Drop parsed dictionaries that are no longer enabled — but only once the
    // cache grows past MAX_LOADED_DICTIONARIES, so a quick toggle-off/on
    // doesn't force a re-parse.
    if st.loaded.len() > MAX_LOADED_DICTIONARIES {
        let enabled: HashSet<String> = st.enabled_codes.iter().cloned().collect();
        st.loaded.retain(|code, _| enabled.contains(code));
    }

    let dir = PathBuf::from(data_dir);
    let dir_changed = dir != st.data_dir;
    st.data_dir = dir;
    if dir_changed || st.personal.is_empty() {
        if !st.data_dir.as_os_str().is_empty() {
            let path = st.data_dir.join("personal.dic");
            st.personal = read_word_list_file(&path);
        }
        // Re-discover so the per-user `hunspell/` dir under the (new) data dir
        // is picked up.
        discover(&mut st);
    }
}

/// Check one block (paragraph) of text. `in_code_fence_before` is the fenced-
/// code-block state inherited from the previous block.
pub(crate) fn spellcheck_check_block(text: &str, in_code_fence_before: bool) -> SpellcheckBlockResult {
    // A block that is entirely inside (or that opens/closes) a fenced code
    // block: do no word checking, just track the fence state.
    let trimmed = text.trim();
    let toggles_fence = trimmed.starts_with("```") || trimmed.starts_with("~~~");
    if in_code_fence_before {
        return SpellcheckBlockResult {
            ranges: Vec::new(),
            in_code_fence_after: !toggles_fence,
        };
    }
    if toggles_fence {
        return SpellcheckBlockResult {
            ranges: Vec::new(),
            in_code_fence_after: true,
        };
    }
    if text.len() > MAX_BLOCK_CHARS {
        return SpellcheckBlockResult {
            ranges: Vec::new(),
            in_code_fence_after: false,
        };
    }

    let mut st = lock();
    if !st.master_enabled || st.enabled_codes.is_empty() {
        return SpellcheckBlockResult {
            ranges: Vec::new(),
            in_code_fence_after: false,
        };
    }

    let skip = skip_ranges(text);
    let mut ranges = Vec::new();
    for (start, end) in word_spans(text) {
        if ranges.len() >= MAX_RANGES_PER_BLOCK {
            break;
        }
        if overlaps_any(start, end, &skip) {
            continue;
        }
        let word = &text[start..end];
        if !looks_checkable(word) {
            continue;
        }
        if is_misspelled(&mut st, word) {
            ranges.push(SpellcheckRange {
                start_utf16: utf16_len(&text[..start]),
                length_utf16: utf16_len(word),
            });
        }
    }
    SpellcheckBlockResult {
        ranges,
        in_code_fence_after: false,
    }
}

/// Correction suggestions for `word`, grouped by source language. Returns no
/// groups if the word is actually spelled correctly (or if nothing is enabled).
pub(crate) fn spellcheck_suggest(word: &str) -> Vec<SpellcheckSuggestionGroup> {
    let mut st = lock();
    let mut groups: Vec<SpellcheckSuggestionGroup> = Vec::new();
    if !st.master_enabled || st.enabled_codes.is_empty() || !looks_checkable(word) {
        return groups;
    }
    if !is_misspelled(&mut st, word) {
        return groups;
    }
    let word_scripts = scripts_of_word(word);
    // Iterate enabled codes in their (already alphabetical, normalised) order;
    // C++ may re-sort by display name but a stable code order is a fine base.
    let codes = st.enabled_codes.clone();
    for code in codes {
        ensure_loaded(&mut st, &code);
        let Some(loaded) = st.loaded.get(&code) else {
            continue;
        };
        if !script_compatible(&loaded.scripts, &word_scripts) {
            continue;
        }
        let Some(dict) = loaded.dict.as_ref() else {
            continue;
        };
        let mut v: Vec<String> = Vec::new();
        dict.suggest(word, &mut v);
        // Drop dupes, the original word, and anything implausibly long; cap it.
        let mut seen = HashSet::new();
        v.retain(|s| {
            !s.is_empty()
                && s != word
                && s.chars().count() <= MAX_WORD_CHARS
                && seen.insert(s.to_lowercase())
        });
        v.truncate(8);
        if !v.is_empty() {
            groups.push(SpellcheckSuggestionGroup {
                language_code: code.clone(),
                suggestions: v,
            });
        }
    }
    groups
}

/// Add `word` to the permanent personal word list and persist it.
pub(crate) fn spellcheck_add_word(word: &str) {
    let word = word.trim();
    if word.is_empty() || word.chars().count() > MAX_WORD_CHARS {
        return;
    }
    let mut st = lock();
    if !st.personal.insert(word.to_lowercase()) {
        return; // already present
    }
    if st.data_dir.as_os_str().is_empty() {
        return;
    }
    let path = st.data_dir.join("personal.dic");
    append_word_to_file(&path, word);
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

fn lock() -> std::sync::MutexGuard<'static, SpellcheckState> {
    state().lock().unwrap_or_else(|e| e.into_inner())
}

/// Whether the given token is shaped like something worth spell-checking at
/// all: long enough, alphabetic-ish, no digits, not an acronym / identifier.
fn looks_checkable(word: &str) -> bool {
    let mut chars = word.chars();
    let Some(first) = chars.next() else {
        return false;
    };
    if word.chars().count() < 2 || word.chars().count() > MAX_WORD_CHARS {
        return false;
    }
    if !first.is_alphabetic() && first != '\'' && first != '\u{2019}' {
        return false;
    }
    let mut has_lower = first.is_lowercase();
    let mut has_inner_upper = false;
    for (i, c) in word.char_indices() {
        // Halfwidth/Fullwidth Forms — Japanese IMEs emit fullwidth Latin (Ａ-Ｚ,
        // ａ-ｚ) in "wide Latin / Zenkaku" mode. Unicode tags those characters
        // Script=Latin, so without this guard they'd be run against the en_US
        // dictionary and flagged as misspellings with no useful suggestions.
        if matches!(c, '\u{FF00}'..='\u{FFEF}') {
            return false;
        }
        if c.is_numeric() {
            return false; // tokens with digits aren't words
        }
        if !(c.is_alphabetic() || c == '\'' || c == '\u{2019}' || c == '-') {
            return false; // some odd symbol slipped in
        }
        if c.is_lowercase() {
            has_lower = true;
        }
        if i > 0 && c.is_uppercase() {
            has_inner_upper = true;
        }
    }
    if has_inner_upper {
        return false; // camelCase / McName / iPhone — likely an identifier
    }
    if !has_lower {
        return false; // ALL CAPS — treat as an acronym, don't flag
    }
    true
}

/// Look the word up across the enabled, script-compatible dictionaries. Also
/// consults the personal and session-ignore lists. Returns true only if it is
/// confidently misspelled.
fn is_misspelled(st: &mut SpellcheckState, word: &str) -> bool {
    let key = word.to_lowercase();
    if st.personal.contains(&key) {
        return false;
    }
    let word_scripts = scripts_of_word(word);
    let mut any_compatible_dict = false;
    let codes = st.enabled_codes.clone();
    for code in &codes {
        ensure_loaded(st, code);
        let Some(loaded) = st.loaded.get(code) else {
            continue;
        };
        if !script_compatible(&loaded.scripts, &word_scripts) {
            continue;
        }
        let Some(dict) = loaded.dict.as_ref() else {
            continue;
        };
        any_compatible_dict = true;
        if dict.check(word) {
            return false;
        }
    }
    // If no enabled dictionary covers this word's script, we have no opinion —
    // don't flag it.
    any_compatible_dict
}

/// Parse and cache the dictionary for `code` if it isn't already.
fn ensure_loaded(st: &mut SpellcheckState, code: &str) {
    if st.loaded.contains_key(code) {
        return;
    }
    // Find a source: prefer a discovered on-disk dictionary, fall back to the
    // built-in if the code matches.
    let on_disk = st
        .discovered
        .iter()
        .find(|d| d.code == code && !d.builtin)
        .and_then(|d| d.path.clone());

    let parsed: Option<Dictionary> = if let Some(dic_path) = on_disk {
        load_from_disk(&dic_path)
    } else if let Some(b) = st.builtin.as_ref().filter(|b| b.code == code) {
        Dictionary::new(&b.aff, &b.dic).ok()
    } else {
        None
    };

    if parsed.is_none() {
        tracing::warn!("spellcheck: failed to load dictionary '{code}'");
    }
    st.loaded.insert(
        code.to_owned(),
        LoadedDictionary {
            dict: parsed,
            scripts: scripts_for_locale(code),
        },
    );
}

fn load_from_disk(dic_path: &Path) -> Option<Dictionary> {
    let aff_path = dic_path.with_extension("aff");
    let aff = std::fs::read_to_string(&aff_path).ok()?;
    let dic = std::fs::read_to_string(dic_path).ok()?;
    Dictionary::new(&aff, &dic).ok()
}

/// (Re)build the list of available dictionaries: the built-in, then everything
/// found under the per-user override dir and the system locations, deduplicated
/// by canonical path. Within a directory, files are visited in sorted order so
/// the chosen code for an aliased dictionary is stable.
fn discover(st: &mut SpellcheckState) {
    let mut out: Vec<DiscoveredDictionary> = Vec::new();
    let mut seen_paths: HashSet<PathBuf> = HashSet::new();
    let mut seen_codes: HashSet<String> = HashSet::new();

    if let Some(b) = st.builtin.as_ref() {
        seen_codes.insert(b.code.clone());
        out.push(DiscoveredDictionary {
            code: b.code.clone(),
            path: None,
            builtin: true,
        });
    }

    for dir in dictionary_search_dirs(&st.data_dir) {
        let Ok(rd) = std::fs::read_dir(&dir) else {
            continue;
        };
        let mut dics: Vec<PathBuf> = rd
            .filter_map(|e| e.ok())
            .map(|e| e.path())
            .filter(|p| p.extension().map(|x| x == "dic").unwrap_or(false))
            .collect();
        dics.sort();
        for dic_path in dics {
            // Need a sibling .aff to be usable.
            if !dic_path.with_extension("aff").exists() {
                continue;
            }
            let canon = std::fs::canonicalize(&dic_path).unwrap_or_else(|_| dic_path.clone());
            let is_alias = canon != dic_path;
            if !seen_paths.insert(canon.clone()) {
                continue; // a different name for a file we already have
            }
            // For symlinked dictionaries (e.g. `en_AG.dic -> en_GB-large.dic`
            // on many distros), name the entry after the *real* file's stem,
            // not the first alphabetical alias — so the list shows
            // "English (United Kingdom)" rather than "English (Antigua &
            // Barbuda)".
            let stem_source: &Path = if is_alias { &canon } else { &dic_path };
            let Some(stem) = stem_source.file_stem().and_then(|s| s.to_str()) else {
                continue;
            };
            let code = normalize_code(stem);
            if code.is_empty() || !seen_codes.insert(code.clone()) {
                continue;
            }
            out.push(DiscoveredDictionary {
                code,
                path: Some(dic_path),
                builtin: false,
            });
        }
    }

    st.discovered = out;
}

/// The directories we look in for `.dic`/`.aff` pairs, highest priority first.
fn dictionary_search_dirs(data_dir: &Path) -> Vec<PathBuf> {
    let mut dirs: Vec<PathBuf> = Vec::new();
    let mut push = |p: PathBuf| {
        if !p.as_os_str().is_empty() && !dirs.contains(&p) {
            dirs.push(p);
        }
    };

    // Per-user override dir: `<data_dir>/hunspell/` — `data_dir` is the
    // profile/app data directory C++ hands us.
    if !data_dir.as_os_str().is_empty() {
        push(data_dir.join("hunspell"));
    }

    if cfg!(target_os = "macos") {
        if let Some(home) = dirs_home() {
            push(home.join("Library/Spelling"));
        }
        push(PathBuf::from("/Library/Spelling"));
    }

    if cfg!(unix) && !cfg!(target_os = "macos") {
        // XDG data dirs (covers Flatpak's Hunspell.* extensions mounted under
        // /usr/share/hunspell as well).
        let xdg_data_home = std::env::var_os("XDG_DATA_HOME")
            .map(PathBuf::from)
            .or_else(|| dirs_home().map(|h| h.join(".local/share")));
        if let Some(h) = xdg_data_home {
            push(h.join("hunspell"));
        }
        let xdg_data_dirs = std::env::var_os("XDG_DATA_DIRS")
            .map(|v| v.to_string_lossy().into_owned())
            .unwrap_or_else(|| "/usr/local/share:/usr/share".to_owned());
        for d in xdg_data_dirs.split(':').filter(|s| !s.is_empty()) {
            push(PathBuf::from(d).join("hunspell"));
        }
        push(PathBuf::from("/usr/share/hunspell"));
        push(PathBuf::from("/usr/share/myspell"));
        push(PathBuf::from("/usr/share/myspell/dicts"));
    }

    dirs
}

fn dirs_home() -> Option<PathBuf> {
    std::env::var_os("HOME").map(PathBuf::from).filter(|p| !p.as_os_str().is_empty())
}

// ---------------------------------------------------------------------------
// Tokenisation / skip ranges
// ---------------------------------------------------------------------------

/// Byte ranges in `text` that should not be word-checked: URLs and emails,
/// inline `` `code` `` spans, `@mentions` / `#tags`, and `:emoji_shortcode:`
/// runs. (Triple-backtick fenced blocks are handled a block at a time by the
/// caller via the fence-state flag, not here.)
fn skip_ranges(text: &str) -> Vec<(usize, usize)> {
    let mut ranges: Vec<(usize, usize)> = Vec::new();

    // URLs + emails.
    let mut finder = LinkFinder::new();
    finder.kinds(&[LinkKind::Url, LinkKind::Email]);
    for link in finder.links(text) {
        ranges.push((link.start(), link.end()));
    }

    // Inline code spans: text between a `` ` `` and the next `` ` ``. Cheap and
    // good enough for a composer — we don't try to honour multi-backtick fences
    // here.
    {
        let bytes = text.as_bytes();
        let mut i = 0usize;
        while i < bytes.len() {
            if bytes[i] == b'`' {
                if let Some(rel) = text[i + 1..].find('`') {
                    let end = i + 1 + rel + 1; // include the closing backtick
                    ranges.push((i, end));
                    i = end;
                    continue;
                }
            }
            i += 1;
        }
    }

    // @mention / #tag / :emoji: — a run starting at one of those sigils up to
    // the next whitespace.
    {
        let bytes = text.as_bytes();
        let mut i = 0usize;
        while i < bytes.len() {
            let c = bytes[i];
            let at_word_start = i == 0 || bytes[i - 1].is_ascii_whitespace();
            if (c == b'@' || c == b'#' || c == b':') && at_word_start {
                let mut j = i + 1;
                while j < bytes.len() && !bytes[j].is_ascii_whitespace() {
                    j += 1;
                }
                if j > i + 1 {
                    ranges.push((i, j));
                }
                i = j;
                continue;
            }
            i += 1;
        }
    }

    ranges.sort_unstable();
    ranges
}

fn overlaps_any(start: usize, end: usize, ranges: &[(usize, usize)]) -> bool {
    ranges.iter().any(|&(s, e)| start < e && s < end)
}

/// Iterator-ish helper: returns byte ranges of "word-like" runs in `text` —
/// maximal runs of alphabetic characters plus intra-word apostrophes and
/// hyphens (leading/trailing apostrophes and hyphens are trimmed back off).
fn word_spans(text: &str) -> Vec<(usize, usize)> {
    let mut out = Vec::new();
    let mut start: Option<usize> = None;
    let is_word_char = |c: char| c.is_alphabetic() || c == '\'' || c == '\u{2019}' || c == '-';
    for (i, c) in text.char_indices() {
        if is_word_char(c) {
            if start.is_none() {
                start = Some(i);
            }
        } else if let Some(s) = start.take() {
            push_trimmed_span(text, s, i, &mut out);
        }
    }
    if let Some(s) = start.take() {
        push_trimmed_span(text, s, text.len(), &mut out);
    }
    out
}

fn push_trimmed_span(text: &str, mut s: usize, mut e: usize, out: &mut Vec<(usize, usize)>) {
    let trim = |c: char| c == '\'' || c == '\u{2019}' || c == '-';
    while s < e {
        let c = text[s..].chars().next().unwrap();
        if trim(c) {
            s += c.len_utf8();
        } else {
            break;
        }
    }
    while e > s {
        let c = text[..e].chars().next_back().unwrap();
        if trim(c) {
            e -= c.len_utf8();
        } else {
            break;
        }
    }
    if e > s {
        out.push((s, e));
    }
}

fn utf16_len(s: &str) -> u32 {
    s.encode_utf16().count() as u32
}

// ---------------------------------------------------------------------------
// Scripts
// ---------------------------------------------------------------------------

/// The distinct "real" scripts present in a word (ignoring `Common`,
/// `Inherited` and `Unknown` — punctuation, combining marks, etc.).
fn scripts_of_word(word: &str) -> Vec<Script> {
    let mut v: Vec<Script> = Vec::new();
    for c in word.chars() {
        let s = c.script();
        if matches!(s, Script::Common | Script::Inherited | Script::Unknown) {
            continue;
        }
        if !v.contains(&s) {
            v.push(s);
        }
    }
    v
}

/// Scripts a dictionary for `locale_code` is responsible for. An empty result
/// means "unknown" — such a dictionary is treated as applicable to any word.
fn scripts_for_locale(locale_code: &str) -> Vec<Script> {
    let lang = locale_code
        .split(|c| c == '_' || c == '-')
        .next()
        .unwrap_or("")
        .to_ascii_lowercase();
    let s = match lang.as_str() {
        // Cyrillic
        "ru" | "uk" | "be" | "bg" | "mk" | "sr" | "ky" | "kk" | "tg" | "mn" => Some(Script::Cyrillic),
        // Greek
        "el" => Some(Script::Greek),
        // Hebrew / Yiddish
        "he" | "yi" => Some(Script::Hebrew),
        // Arabic-script languages
        "ar" | "fa" | "ur" | "ps" | "ckb" | "sd" => Some(Script::Arabic),
        "hy" => Some(Script::Armenian),
        "ka" => Some(Script::Georgian),
        "am" | "ti" => Some(Script::Ethiopic),
        "hi" | "mr" | "ne" | "sa" => Some(Script::Devanagari),
        "bn" => Some(Script::Bengali),
        "ta" => Some(Script::Tamil),
        "th" => Some(Script::Thai),
        "ko" => Some(Script::Hangul),
        // Everything else with a Hunspell dictionary is Latin in practice
        // (en, de, fr, es, it, pt, nl, pl, cs, sk, hu, ro, tr, vi, …).
        "" => None,
        _ => Some(Script::Latin),
    };
    s.into_iter().collect()
}

/// Whether a dictionary covering `dict_scripts` should be consulted for a word
/// whose scripts are `word_scripts`. Unknown-script dictionaries (empty
/// `dict_scripts`) and scriptless words are always considered compatible.
fn script_compatible(dict_scripts: &[Script], word_scripts: &[Script]) -> bool {
    if dict_scripts.is_empty() || word_scripts.is_empty() {
        return true;
    }
    word_scripts.iter().any(|w| dict_scripts.contains(w))
}

// ---------------------------------------------------------------------------
// Misc helpers
// ---------------------------------------------------------------------------

/// Normalise a dictionary code/stem to the conventional `ll` or `ll_CC` form:
/// lower-case language, upper-case region, `_` separator, and SCOWL-style
/// size/spelling suffixes dropped (`en_GB-large` → `en_GB`, `en-US` → `en_US`,
/// `de_DE_frami` → `de_DE`). Returns `""` when the stem isn't a plausible
/// locale code.
fn normalize_code(raw: &str) -> String {
    let raw = raw.trim();
    if raw.is_empty() {
        return String::new();
    }
    let is_sep = |c: char| c == '_' || c == '-';
    // First separator (whichever of '_' / '-' appears first) is the
    // language/region boundary; everything from the next separator on is a
    // suffix we don't care about.
    let (lang, rest) = match raw.find(is_sep) {
        Some(i) => (&raw[..i], Some(&raw[i + 1..])),
        None => (raw, None),
    };
    let lang = lang.to_ascii_lowercase();
    if lang.is_empty() || !lang.chars().all(|c| c.is_ascii_alphabetic()) {
        return String::new();
    }
    match rest {
        Some(rest) => {
            let region: &str = match rest.find(is_sep) {
                Some(i) => &rest[..i],
                None => rest,
            };
            if region.is_empty() || !region.chars().all(|c| c.is_ascii_alphanumeric()) {
                lang
            } else {
                format!("{lang}_{}", region.to_ascii_uppercase())
            }
        }
        None => lang,
    }
}

fn read_word_list_file(path: &Path) -> HashSet<String> {
    let Ok(contents) = std::fs::read_to_string(path) else {
        return HashSet::new();
    };
    contents
        .lines()
        .map(|l| l.trim())
        .filter(|l| !l.is_empty() && !l.starts_with('#'))
        .map(|l| l.to_lowercase())
        .collect()
}

fn append_word_to_file(path: &Path, word: &str) {
    use std::io::Write;
    if let Some(parent) = path.parent() {
        let _ = std::fs::create_dir_all(parent);
    }
    if let Ok(mut f) = std::fs::OpenOptions::new().create(true).append(true).open(path) {
        let _ = writeln!(f, "{word}");
    }
}

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn normalize_code_variants() {
        assert_eq!(normalize_code("en_US"), "en_US");
        assert_eq!(normalize_code("en-US"), "en_US");
        assert_eq!(normalize_code("en_GB-large"), "en_GB");
        assert_eq!(normalize_code("en_GB-ize"), "en_GB");
        assert_eq!(normalize_code("bg_BG"), "bg_BG");
        assert_eq!(normalize_code("de"), "de");
        assert_eq!(normalize_code(""), "");
        assert_eq!(normalize_code("123"), "");
    }

    #[test]
    fn word_spans_basic() {
        let t = "Hello, world! It's a test—really.";
        let spans: Vec<&str> = word_spans(t).into_iter().map(|(s, e)| &t[s..e]).collect();
        assert_eq!(spans, vec!["Hello", "world", "It's", "a", "test", "really"]);
    }

    #[test]
    fn looks_checkable_filters() {
        assert!(looks_checkable("hello"));
        assert!(looks_checkable("It's"));
        assert!(looks_checkable("Hello"));
        assert!(!looks_checkable("a")); // too short
        assert!(!looks_checkable("HTTP")); // all caps
        assert!(!looks_checkable("getElementById")); // camelCase
        assert!(!looks_checkable("iPhone")); // inner caps
        assert!(!looks_checkable("h2o")); // has a digit
    }

    #[test]
    fn looks_checkable_rejects_fullwidth_forms() {
        // Zenkaku (fullwidth Latin) is what a Japanese IME emits in "wide Latin"
        // mode — Unicode marks it Script=Latin, but it isn't an English word.
        assert!(!looks_checkable("ｈｅｌｌｏ")); // fullwidth lowercase
        assert!(!looks_checkable("Ｈｅｌｌｏ")); // fullwidth title-case
        assert!(!looks_checkable("ｈello")); // mixed — still CJK-context
        assert!(looks_checkable("hello")); // regression: normal ASCII still checks
    }

    #[test]
    fn skip_ranges_catch_urls_mentions_code() {
        let t = "see https://example.org and @alice:server and `code` and :smile:";
        let r = skip_ranges(t);
        // The word "example" inside the URL must be covered.
        let idx = t.find("example").unwrap();
        assert!(overlaps_any(idx, idx + 7, &r));
        // "alice" inside the mention.
        let idx = t.find("alice").unwrap();
        assert!(overlaps_any(idx, idx + 5, &r));
        // "code" inside the backticks.
        let idx = t.find("code").unwrap();
        assert!(overlaps_any(idx, idx + 4, &r));
        // "smile" inside the shortcode.
        let idx = t.find("smile").unwrap();
        assert!(overlaps_any(idx, idx + 5, &r));
    }

    #[test]
    fn bundled_en_us_dictionary_parses_and_checks() {
        // The bundled fallback dictionary must actually load with spellbook and
        // give sane answers — otherwise the composer would silently flag
        // nothing.
        let aff = include_str!("../../../resources/dictionaries/en_US.aff");
        let dic = include_str!("../../../resources/dictionaries/en_US.dic");
        let dict = Dictionary::new(aff, dic).expect("bundled en_US dictionary must parse");
        assert!(dict.check("hello"));
        assert!(dict.check("Hello"));
        assert!(dict.check("computer"));
        assert!(!dict.check("helllo"));
        assert!(!dict.check("teh"));
        let mut sugg = Vec::new();
        dict.suggest("helllo", &mut sugg);
        assert!(sugg.iter().any(|s| s == "hello"), "expected 'hello' among {sugg:?}");
    }

    #[test]
    fn scripts_basics() {
        assert_eq!(scripts_of_word("hello"), vec![Script::Latin]);
        assert_eq!(scripts_of_word("don't"), vec![Script::Latin]);
        assert_eq!(scripts_of_word("здравей"), vec![Script::Cyrillic]);
        assert!(script_compatible(&[Script::Latin], &[Script::Latin]));
        assert!(!script_compatible(&[Script::Latin], &[Script::Cyrillic]));
        assert!(script_compatible(&[], &[Script::Cyrillic])); // unknown dict
        assert_eq!(scripts_for_locale("en_US"), vec![Script::Latin]);
        assert_eq!(scripts_for_locale("bg_BG"), vec![Script::Cyrillic]);
    }
}
