# 🔤 Spell Checking

Komai's spell-checker is an offline Hunspell engine running in Rust, with a
thin C++/QML attach layer that paints the squiggles. This page covers the
internals; for the end-user-visible behaviour see the [user guide](../user-guide/features/spellcheck.md).

## Engine (Rust)

- `src/rust/src/spellcheck.rs` — `spellcheck_*` cxx-bridge functions.
- Backed by the [`spellbook`](https://crates.io/crates/spellbook) crate (a
  pure-Rust Hunspell reimplementation), plus [`unicode-script`](https://crates.io/crates/unicode-script)
  for filtering suggestions to script-compatible dictionaries.
- No system C dependency — packaging only needs Rust.

A word is reported misspelled only when **no** enabled, script-compatible
dictionary recognises it. With English and Bulgarian both enabled, a message
mixing the two raises no false positives.

`spellcheck_check_block` runs per `QTextBlock` and is fed the previous block's
fenced-code state so multi-line ```/~~~ fences suppress checking until they
close.

### Dictionary discovery

The bundled `en_US.{aff,dic}` ships under `:/dictionaries/` in the Qt resource
bundle (see `resources/dictionaries/` and `res.qrc`) and is registered at
process start via `spellcheck_register_builtin_dictionary`. It is always
available, so spell-check works on every platform with no setup.

Additional dictionaries are discovered from the filesystem on demand by
`spellcheck_discover_dictionaries`:

- `<dataDir>/hunspell/` (per-user override dir, manually populated)
- `/usr/share/hunspell` and `/usr/share/myspell/dicts` (Linux system)
- Each `$XDG_DATA_DIRS/hunspell` entry
- `~/Library/Spelling` (macOS — no system Hunspell dir, manual only)

Inside a Flatpak, `org.freedesktop.Platform.Hunspell.<locale>` extensions
mount at `/usr/share/hunspell` and are picked up by the same code path. The
manifest declares an `add-extensions` block so language packs flow in via
`flatpak install` rather than being rebuilt into the runtime. The Arch
PKGBUILD declares `hunspell-*` packages as optional dependencies for the same
reason.

### Word filtering (skip rules)

Before running each token through the dictionary, the Rust side drops:

- URLs and email addresses
- Backtick-delimited code spans, fenced code blocks
- `@mentions`, `#tags`, `:emoji:` shortcodes
- All-caps acronyms
- `camelCase` / identifier-looking tokens
- CJK runs (Hunspell is the wrong tool — see [Out of Scope](#out-of-scope))

The Rust unit tests cover each rule (`spellcheck::tests::*` in `src/rust/src/spellcheck.rs`).

## C++ attach layer

Two singletons in `src/spellcheck/`:

- `SpellCheckEngine` — process-global facade. Owns the configuration (master
  toggle + enabled language list, both read from `UserSettings`), drives the
  Rust engine, and exposes the right-click suggestion menu and personal-dict
  helpers to QML.
- `SpellChecker` — one per attached `QTextDocument`. Listens for
  `contentsChange`, debounces ~200 ms, and re-paints the squiggles.

`SpellChecker` is attached from QML on the two text surfaces that support it:

- The composer textarea (`resources/qml/composer/MessageInput.qml`)
- The room-topic editor (`resources/qml/dialogs/room/tabs/RoomInfoSettingsTab.qml`)

### Squiggle rendering

The squiggle is a `QTextCharFormat::SingleUnderline` written straight into the
document with `QTextCursor::mergeCharFormat`. The wave-underline style is not
rendered by the Qt Quick text renderer — a flat red underline reads as
"misspelled" clearly enough. (A `QSyntaxHighlighter` overlay format does not
render at all in the Qt Quick text editor; that path was tried and abandoned.)

Two practical wrinkles that affect the format-based approach:

1. **Whole-document clear before each repaint.** When the user keeps typing
   right after a misspelled word, `QTextDocument` copies the preceding
   character's format onto the freshly inserted run, so the squiggle "fans
   out" beyond the range we originally marked. A range-limited clear (over
   only the previously cached spans) would miss the leak. The recheck pass
   selects the whole document, merges `NoUnderline` + `setFontUnderline(false)`
   first, then re-applies squiggles. This is safe because the composer has
   no other source of underline formatting.

2. **Eager clear on `contentsChange`.** Even with the whole-document clear,
   the leaked underline is visible for ~200 ms (the debounce window). The
   `contentsChange` handler immediately strips the underline format from the
   just-inserted `[position, position + charsAdded)` range, so newly typed
   text never *visibly* inherits the fan-out. The debounced Rust recheck then
   re-paints proper squiggles. Net effect: new misspellings get a squiggle
   after ~200 ms; correctly-typed continuations never flash a stray squiggle.

The `applyingFormats_` flag guards both passes from re-entering each other
through `contentsChange`.

### Right-click menu

`SpellChecker::misspelledWordAround(position)` walks the cached
`underlinedRanges_` list and returns `{found, word, start, length}` if the
cursor position falls inside a flagged span. `MessageContextMenu.qml` /
`TopicTextField.qml` use that to switch between the generic Cut/Copy/Paste
menu and the spelling-suggestions menu. Suggestions come from
`spellcheck_suggest` and are grouped by source language so users with multiple
dictionaries enabled see e.g. `Spellcheck (English) [3] ▸` and
`Spellcheck (Bulgarian) [2] ▸` submenus.

## Configuration

Two settings live in the per-profile `config.yml`:

- `composer.input.spellcheck.enabled` (bool, default `true`) — master toggle.
  Persists through the normal `SettingsStore` → `UserSettings` →
  `setComposerInputSpellcheckEnabled` path (descriptor row in
  `src/settings/ui/rows/UserSettingsModelComposer.inc`).
- `composer.input.spellcheck.languages` (sequence of strings, default `[]`)
  — alphabetically sorted normalised locale codes (`bg_BG`, `en_US`, …).
  Bypasses `SettingsStore` for the same reason `timeline.hidden_events.global`
  does — `SettingsStore::Value` is a single-value variant, list shapes
  round-trip through the serializer snapshot directly.

The Rust side parses both via `parse_scalar_bool` / `parse_string_list` in
`src/rust/src/settings/config/mod.rs`; the YAML bridge emits the list as a
`Value::Sequence`.

### First-run seeding

If `composer.input.spellcheck.languages` is empty when `SpellCheckEngine`
constructs, it derives a default from `QLocale::system().uiLanguages()`
intersected with the discovered dictionary set (plus `en_US` as a backstop
for the inevitable loanwords), normalises and sorts it, and writes it back
through `UserSettings::setComposerInputSpellcheckLanguages`. The default
becomes a concrete value on disk so the user sees a real list in their
`config.yml`.

### Per-user, not per-profile

The personal word list (`personal.dic`) and the manual-install dictionary
directory (`hunspell/`) live directly under the app data root
(`~/.local/share/komai/` and platform equivalents), **not** under the
per-profile config dir. They're "things this human uses" rather than
"per-account state". The Rust side joins `<dataDir>/personal.dic` for the
personal list and treats `<dataDir>/hunspell/` as one of the dictionary
discovery roots.

## Out of scope

These were deliberately not built — each is a feature trap with a poor cost/
benefit ratio for Komai's current scope:

- **CJK languages.** Hunspell is a stemmed-word checker; Chinese, Japanese,
  and Korean don't have whitespace-delimited words in the same sense, and
  there are no Hunspell dictionaries for them in distros. CJK runs are
  filtered out before checking, so they never produce a flag.
- **Per-room overrides.** Spellcheck is a typing aid, not a content policy;
  no compelling per-room use case.
- **In-app dictionary download.** Users install Hunspell packs through their
  distro's package manager / Flatpak / LibreOffice; bundling a downloader
  would mean shipping mirror URLs and signature validation for a use case
  most users already have handled elsewhere.
- **Per-token language identification.** With multiple dictionaries enabled,
  a word is accepted if any of them recognise it. Trying to guess "this
  specific token is English vs Bulgarian" produces flicker and false
  positives on loanwords and proper nouns. The any-accepts model is robust
  with no UX cost.

## Related

- [User guide: Spell checking](../user-guide/features/spellcheck.md)
- [Rust in Komai](rust.md) — CXX-bridge pattern
- [Settings Architecture](settings/README.md) — how `config.yml` flows
- `src/spellcheck/` — C++ layer
- `src/rust/src/spellcheck.rs` — Rust engine
