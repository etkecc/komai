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

Plural messages are currently **skipped** by the translation pipeline. They require special handling because:

- Different languages have different numbers of plural forms (1 for Japanese/Chinese/Turkish, 2 for most European languages, 3 for Czech/Polish/Russian, 6 for Arabic).
- Each form needs a separate translation in a `<numerusform>` element.
- The AI prompt would need to explain the specific plural rules for each language.

The script reports the count of skipped plural forms so users are aware. This is a known limitation to be addressed in a future iteration.


## Rust-originated strings

The Rust backend generates user-facing strings for timeline state events, event type labels, error messages, and more. These are translated on the C++ side using Qt's `tr()` mechanism, keeping a single translation system.

### How it works

Rust passes **structured data** (machine-readable keys + parameters) through the cxx FFI bridge instead of pre-formatted English sentences. C++ translation modules map these keys and parameters to translated strings via `QCoreApplication::translate()`.

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

### Adding new Rust-originated translatable strings

1. Have Rust populate structured fields (keys + parameters) — don't construct English sentences
2. Add the corresponding `tr()` call in the appropriate C++ translation module
3. Run `just translations-update` — `lupdate` picks up the new `tr()` calls automatically
4. Run `just translations-claude-translate-all` to translate


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
