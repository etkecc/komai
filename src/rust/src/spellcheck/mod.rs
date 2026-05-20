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

mod io;
mod scripts;
mod text;

use io::{append_word_to_file, dictionary_search_dirs, read_word_list_file};
use scripts::{normalize_code, script_compatible, scripts_for_locale, scripts_of_word};
use text::{looks_checkable, overlaps_any, skip_ranges, utf16_len, word_spans};

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


#[cfg(test)]
mod tests;
