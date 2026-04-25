# Translation system architecture

This document covers the technical design of the AI-assisted translation pipeline in `bin/translations/translate.py` and the decisions behind it.

For user-facing workflow documentation, see [docs/maintainers/translations.md](../maintainers/translations.md).


## Qt .ts file format

Qt Linguist uses XML `.ts` files to store translations. Each file contains `<context>` blocks (one per QML component or C++ class), with `<message>` elements holding the source string, translation, and source location:

```xml
<context>
    <name>RoomList</name>
    <message>
        <location filename="../qml/RoomList.qml" line="+42"/>
        <source>Find &amp; switch room</source>
        <translation>Raum suchen &amp; wechseln</translation>
    </message>
</context>
```

Untranslated strings have `<translation type="unfinished">`. Plural forms use `<message numerus="yes">` with `<numerusform>` children inside the translation element.


## XML normalization

### The problem

Two tools write `.ts` files: Qt's `lupdate` (extracts source strings) and our translation script (fills in translations). They serialize XML differently:

| Aspect | `lupdate` output | ElementTree output |
|--------|------------------|--------------------|
| Single quotes | `&apos;` | `'` |
| Double quotes in text | `&quot;` | `"` |
| Empty elements | `<translation type="unfinished"></translation>` | `<translation type="unfinished"/>` |
| Self-closing tags | `<location line="+5"/>` | `<location line="+5" />` |

Both forms are valid XML and semantically identical, but the difference means that running `lupdate` after the translation script (or vice versa) would produce spurious diffs touching hundreds of lines, making code review of actual translation changes impractical.

### The solution

We normalize all `.ts` files to ElementTree's canonical form. The cycle is:

```
lupdate writes .ts  -->  normalize (ET round-trip)  -->  stable canonical form
```

Key properties:

- **Idempotent**: normalizing an already-normalized file produces no changes.
- **Stable with `lupdate`**: the cycle `normalized file -> lupdate -> normalize` produces the exact same file (verified by byte-identical comparison). The only things `lupdate` changes are entity encoding style and self-closing element expansion, all of which normalize reverses.
- **`lupdate` path recalculation**: `lupdate` rewrites relative `filename=` attributes in `<location>` elements based on the current working directory. This is unavoidable and happens regardless of normalization, but it's also normalized consistently by the ET round-trip.

The `just translations-update` command runs `lupdate` followed by `normalize` automatically, so the committed files are always in canonical form.

### Self-closing tag style

ElementTree adds a space before `/>` in self-closing tags (`<location line="+5" />`), while `lupdate` omits it (`<location line="+5"/>`). We post-process ET's output with `re.sub(r" />", "/>", body)` to match the more compact style. This is a cosmetic choice — either form is valid XML.


## Translation injection via ElementTree

### Why not regex-based injection

The original implementation used line-by-line regex matching to find `<source>` and `<translation type="unfinished">` elements, then replaced the translation line in-place. This approach had several bugs:

1. **Multi-line `<source>` elements**: 24 source strings in the codebase span multiple lines (e.g., long tooltips with `\n`). The single-line regex `r"\s*<source>(.*?)</source>"` couldn't match these, so translations for those strings were silently lost.

2. **Numerus (plural) forms**: Plural messages have a multi-line `<translation>` with `<numerusform>` children. The single-line unfinished-translation regex couldn't match these either, leading to silent injection failures.

3. **Duplicate source strings across contexts**: The same source string (e.g., "Close", "Forward") can appear in multiple QML/C++ contexts. The regex approach tracked a single `current_source` and matched it against a flat `source -> context` dict, so only one context received the translation.

### Current approach

The script now uses ElementTree for both parsing and modification:

```python
tree = ET.parse(ts_path)
root = tree.getroot()
for context_elem in root.findall("context"):
    for message in context_elem.findall("message"):
        translation_elem = message.find("translation")
        if translation_elem.get("type") == "unfinished":
            translation_elem.text = translated_text
            del translation_elem.attrib["type"]
write_ts(ts_path, root)
```

This handles multi-line sources, nested elements, and entity encoding automatically. The `write_ts` function preserves the XML declaration/DOCTYPE header and applies self-closing tag normalization.


## Plural forms (numerus)

Plural messages (`<message numerus="yes">`) are translated through a parallel pipeline that runs alongside the regular pass — they share the LLM CLI, prompt-loading, and XML normalization but use a different output shape (`forms: [str, ...]` instead of a single `translation`).

### Key insight: Qt encoded CLDR; we just count slots

The whole pipeline rests on one observation: **we don't need to know CLDR plural rules ourselves**. Qt's `lupdate` already knows them — when it generates a `.ts` file for a language, it pre-populates each numerus message with exactly the right number of empty `<numerusform/>` children, in the language's CLDR canonical slot order (`zero`, `one`, `two`, `few`, `many`, `other`, filtered to the categories Qt actually emits for that language). Our job is just to **fill the slots Qt already laid out**, in order.

Counts as of Qt 6:

| Forms | Languages |
|------:|-----------|
| 1 | fa, hu, id, ja, ko, tr, vi, zh_CN, zh_Hant |
| 2 | ca, de, el, en, eo, es, et, fi, fr, ie, it, ml, nl, pt_BR, pt_PT, si, sv |
| 3 | cs, pl, ro, ru, sr_Latn, uk |
| 6 | ar |

Verify any single language's slot count by counting `<numerusform` inside the first `<message numerus="yes">` block of its `.ts` file — that's what `extract_unfinished_numerus()` does at runtime.

### LANG_FORMS: per-language slot labels

`LANG_FORMS` in `translate.py` maps each language code to the ordered list of CLDR category labels for its slots. The model needs the labels because "form 2" alone is ambiguous — depending on the language it could mean `other`, `few`, or `one` (in Arabic). Examples:

```python
LANG_FORMS = {
    "ja": ["other"],                                              # 1-form
    "de": ["one", "other"],                                       # 2-form
    "ru": ["one", "few", "many"],                                 # 3-form, Slavic
    "cs": ["one", "few", "other"],                                # 3-form, "other"-tail
    "ar": ["zero", "one", "two", "few", "many", "other"],         # 6-form
    ...
}
```

Two non-obvious wrinkles encoded in this map:

- **3-form Slavic-style vs `other`-tail.** Languages like Russian, Polish, and Ukrainian use `["one", "few", "many"]` because their third slot covers the bulk of integers (CLDR's `many`); CLDR's `other` only applies to fractions there and Qt skips emitting a slot for it. Czech, Romanian, and Serbian (Latin) get `["one", "few", "other"]` instead — same form count, different last-slot meaning. Don't auto-derive 3-form labels from the count alone.

- **Qt-collapsed languages (fa, hu, tr, possibly id).** CLDR formally lists 2 plural categories for these, but Qt emits a single slot — a long-standing convention treating them as effectively single-form for UI strings. `LANG_FORMS` reflects what Qt actually does, not what CLDR says: each gets `["other"]` (the CLDR fallback category).

`get_form_categories(lang, form_count)` validates the static map against the live slot count on every call. A mismatch raises immediately — that's how we'd find out if a Qt update changed plural rules for a language, or if a new language was added without a `LANG_FORMS` entry.

### Pipeline shape

| Function | Role |
|----------|------|
| `extract_unfinished_numerus(ts_path)` | Find `<message numerus="yes">` blocks with `type="unfinished"`; report each entry's `form_count`. |
| `build_numerus_prompt(batch, lang, instructions, form_categories)` | Per-language prompt naming each slot's CLDR category; asks the model for a `forms` array. |
| `translate_batch_numerus(batch, lang, instructions, model)` | Calls the LLM, parses the response, runs `validate_translation()` on each form independently. |
| `inject_numerus_translations(ts_path, translations)` | Writes back into existing slots in order; clears `type="unfinished"`. |

The driver (`cmd_translate`) runs both passes by default; `--regular-only` and `--numerus-only` switch between them.

Per-language `GUIDE.md` files include a `## Plural forms` section that names each slot's CLDR rule (e.g., for Russian: "form 1 (one) for n%10==1 && n%100!=11; form 2 (few) for n%10 in 2..4 && n%100 not in 12..14; form 3 (many) for everything else"). This gives the model language-specific guidance beyond what the generic prompt provides; the common `GUIDE.md` rule #15 explains the `forms` output shape.

### Failure modes and re-run semantics

The numerus path is incremental and idempotent in the same way as the regular pass:

- **Form-count mismatch** (model returns the wrong number of forms) — the inject step logs and skips that entry; the message stays `type="unfinished"` and gets re-processed on the next run.
- **Per-form validator rejection** (`%n` missing from one form, dropped HTML, mangled shortcut) — `translate_batch_numerus()` rejects the whole entry rather than committing a partially-broken set; same re-run-recovers semantics.
- **Batch failure** (LLM returns garbage JSON, CLI timeout) — the existing per-batch error handling in `_run_translation_pass` applies; the batch is logged as failed and unfinished entries roll over to the next invocation.

There's no rollback for already-injected forms. If the model produces grammatically wrong but technically valid output (preserves `%n`, right form count), it lands in the file and needs human review. The shakedown phase (`de` → `ru` → `ar` covering 2/3/6-form complexity) exists specifically to catch prompt-template defects before a 32-language bulk run.


## Rust-originated strings

The Rust backend generates user-facing strings for timeline state events, event type labels, notifications, error messages, and more. These are translated on the C++ side using Qt's `tr()` mechanism, keeping a single translation system.

### Core principle

**Rust extracts facts. C++ constructs sentences.** Rust passes structured data (machine-readable keys + parameters) through the cxx FFI bridge. C++ translation modules map these to translated strings via `QCoreApplication::translate()`. `lupdate` picks up the `tr()` calls automatically, and the existing AI translation pipeline translates them.

Data flow:

```
Rust (e.g. runtime_event_summary.rs)
  ├── item_kind: "membership_change"
  ├── membership_change_kind: "banned"
  ├── state_event_target_user: "Alice"
  ├── state_event_reason: "spam"
  └── state_event_has_sender: true
        │
        ▼  (cxx FFI bridge)
C++ translation module (e.g. StateEventText.cpp)
  └── tr("%1 was banned by %2: %3").arg("Alice", "Bob", "spam")
        │
        ▼  (lupdate extracts tr() calls)
.ts files → translated by AI pipeline → .qm at runtime
```

### Categories of Rust strings

| Category | Rust source | C++ translation | Notes |
|----------|------------|-----------------|-------|
| Timeline state events | `runtime_event_summary.rs` sets `item_kind` + parameter fields | `StateEventText::translate()` | ~60 sentence variants for membership, profile, room state |
| Event type labels | `runtime_event_summary.rs` sets `item_kind` | `StateEventText::eventTypeLabel()` | "[Poll]", "Deleted message", etc. |
| Notifications | `runtime_notifications.rs` sets `notification_kind` | `StateEventText::translateNotificationBody()` | Invite text, event labels in notifications |
| Room list previews | `runtime_room_list.rs` sets `last_message_kind` | `StateEventText::translateRoomListPreview()` | Reuses `eventTypeLabel()` + state event labels |
| Verification UI | `runtime_verification.rs` sends enum-like keys | QML `qsTr()` + C++ enums | State names are internal keys; user text is in QML |
| SAS emoji descriptions | Defined in QML `EmojiVerification.qml` | `qsTr()` in QML | 64 emoji labels from the Matrix SAS spec |
| Auth/registration errors | `auth.rs`, `registration.rs` | `StateEventText::translateAuthError()` | String-matching approach; see below |

### Robustness

Since the Rust↔C++ key contract can't be enforced at compile time, two measures prevent silent failures:

1. **C++ catch-all fallbacks**: Every translation function has a catch-all that returns a generic translated message for unrecognised keys (e.g., `"Membership updated for %1"`, `"Room state changed by %1"`). State events never produce empty display text.

2. **Cross-reference comments in Rust**: Each Rust summary/error function has a doc comment pointing to its C++ translation counterpart (e.g., `/// Translated to user-visible text in C++ StateEventText::translateMembershipChange(). When adding or changing keys, update that function too.`).

Rust does NOT generate English fallback text in `body` for state events. The `body` field is empty for state events -- all display text comes from C++.

### Auth/registration errors

Auth errors use a different pattern from the key-based approach. Errors flow as `Result<_, String>` through FFI, and most C++ consumers already wrap them in `tr("Failed to X: %1").arg(error)`. For the few display sites that show raw Rust errors (login/registration pages, room directory), `StateEventText::translateAuthError()` maps known constant strings and recognised dynamic prefixes to `tr()` calls via string matching.

### Key files

| File | Role |
|------|------|
| `src/rust/src/matrix_backend/runtime_event_summary.rs` | Extracts structured data from Matrix events |
| `src/rust/src/ffi.rs` | FFI structs with state event / notification parameter fields |
| `src/timeline/StateEventText.cpp` | Central C++ translation module with `tr()` calls |
| `src/timeline/StateEventText.h` | Header for all translation functions |
| `src/timeline/rust/MatrixTimelineModel.cpp` | Calls `StateEventText::translate()` in `computeDerivedFields()` |
| `src/chat/ChatPageCore.cpp` | Calls `translateNotificationBody()` for notifications |
| `src/timeline/data/RoomlistModelData.cpp` | Calls `translateRoomListPreview()` for room list |
| `src/auth/LoginPage.cpp`, `RegisterPage.cpp` | Call `translateAuthError()` for auth errors |
| `resources/qml/device-verification/EmojiVerification.qml` | SAS emoji descriptions with `qsTr()` |

### Adding new Rust-originated translatable strings

1. Have Rust populate structured fields (keys + parameters) -- don't construct English sentences
2. Add the corresponding `tr()` call in the appropriate C++ translation module
3. Add a cross-reference comment on the Rust function pointing to the C++ counterpart
4. Run `just translations-update` -- `lupdate` picks up the new `tr()` calls automatically
5. Run `just translations-claude-translate-all` to translate


## Claude CLI integration

### Prompt structure

Each batch is sent as a single prompt containing:

1. The general translation guide (`resources/langs/GUIDE.md`)
2. Optional per-language guide (`resources/langs/{lang}/GUIDE.md`)
3. The JSON array of `{source, context}` objects
4. Instructions to return a JSON array of `{source, translation}` objects

The `context` field (QML component or C++ class name) helps Claude disambiguate short strings like "Call" (VoIP context vs. Settings context).

### Batch processing

Strings are processed in batches (default: 75) to stay within context limits. Each batch is saved immediately after injection, so:

- Aborting mid-run (Ctrl+C) preserves all completed batches.
- Re-running processes only remaining unfinished strings.
- Errors on one batch stop the run but don't lose prior work.

### Response parsing

Claude's response is parsed with fallback strategies:

1. Direct JSON parse (ideal case)
2. Extract from markdown code fence (` ```json ... ``` `)
3. Find the outermost `[...]` bracket pair

### Duplicate source handling

The same source string can appear in multiple contexts (e.g., "Close" in `ForwardCompleter`, `QuickSwitcher`, and `StickerPicker`). The prompt sends deduplicated sources, but the script maps each returned translation to all contexts that share that source string, so all instances get translated in one pass.
