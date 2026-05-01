#!/usr/bin/env python3
"""Manage Komai translations: normalize .ts files and auto-translate via an LLM.

Subcommands:
    normalize   Normalize .ts files to a canonical XML format (idempotent).
    translate   Translate unfinished strings for a language using an LLM.

Examples:
    python3 bin/translations/translate.py normalize
    python3 bin/translations/translate.py normalize --lang de
    python3 bin/translations/translate.py translate de
    python3 bin/translations/translate.py translate ja --batch-size 50
    python3 bin/translations/translate.py translate fr --dry-run

The translate subcommand:
1. Parses resources/langs/<lang>/komai_<lang>.ts for unfinished translations
2. Sends batches of source strings to the configured LLM for translation
3. Injects each batch back into the .ts file immediately (incremental save)
4. On re-run, only processes remaining unfinished strings

The current LLM integration uses the `claude` CLI — see `call_claude()`. If
you need a different provider, swap that one function; the rest of the
pipeline is provider-neutral.
"""

import argparse
import glob
import json
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from collections import Counter

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.join(SCRIPT_DIR, "..", "..")
LANGS_DIR = os.path.join(PROJECT_ROOT, "resources", "langs")
GUIDE_FILE = os.path.join(LANGS_DIR, "GUIDE.md")

# Map language codes to human-readable names for the prompt
LANGUAGE_NAMES = {
    "ar": "Arabic",
    "ca": "Catalan",
    "cs": "Czech",
    "de": "German",
    "el": "Greek",
    "en": "English",
    "eo": "Esperanto",
    "es": "Spanish",
    "et": "Estonian",
    "fa": "Persian (Farsi)",
    "fi": "Finnish",
    "fr": "French",
    "hu": "Hungarian",
    "id": "Indonesian",
    "ie": "Interlingue",
    "it": "Italian",
    "ja": "Japanese",
    "ko": "Korean",
    "ml": "Malayalam",
    "nl": "Dutch",
    "pl": "Polish",
    "pt_BR": "Brazilian Portuguese",
    "pt_PT": "European Portuguese",
    "ro": "Romanian",
    "ru": "Russian",
    "si": "Sinhala",
    "sr_Latn": "Serbian (Latin)",
    "sv": "Swedish",
    "tr": "Turkish",
    "uk": "Ukrainian",
    "vi": "Vietnamese",
    "zh_CN": "Simplified Chinese",
    "zh_Hant": "Traditional Chinese",
}


# Per-language CLDR plural-form labels, in the order Qt's lupdate emits
# <numerusform/> slots. The number of labels here MUST equal the number of
# slots Qt emits for that language; that count is also discoverable at
# runtime by counting <numerusform> elements inside any <message numerus="yes">
# block. Source: CLDR canonical category order (zero, one, two, few, many,
# other), filtered to the categories Qt actually emits for integer counts
# in each language. Cross-checked against the slot counts in
# resources/langs/<lang>/komai_<lang>.ts as of 2026-04-25.
#
# A few notes on the multi-form languages:
# - pl/ru/uk: Slavic-style 3-form. The third slot is "many" (the bulk of
#   integers including 0); CLDR's "other" only applies to fractions and
#   so isn't emitted by lupdate for these languages.
# - cs/ro/sr_Latn: 3-form but the third slot is "other" (CLDR's "many"
#   either doesn't apply or is decimal-only).
# - fa/hu/tr: CLDR formally lists 2 plural categories, but Qt emits a
#   single slot — historic Qt convention treats these as effectively
#   single-form for UI strings. The lone slot is labelled "other"
#   (the CLDR fallback category).
LANG_FORMS = {
    # 1-form (single translation covers all counts)
    "fa": ["other"],
    "hu": ["other"],
    "id": ["other"],
    "ja": ["other"],
    "ko": ["other"],
    "tr": ["other"],
    "vi": ["other"],
    "zh_CN": ["other"],
    "zh_Hant": ["other"],
    # 2-form
    "bg": ["one", "other"],
    "ca": ["one", "other"],
    "de": ["one", "other"],
    "el": ["one", "other"],
    "en": ["one", "other"],
    "eo": ["one", "other"],
    "es": ["one", "other"],
    "et": ["one", "other"],
    "fi": ["one", "other"],
    "fr": ["one", "other"],
    "ie": ["one", "other"],
    "it": ["one", "other"],
    "ml": ["one", "other"],
    "nl": ["one", "other"],
    "pt_BR": ["one", "other"],
    "pt_PT": ["one", "other"],
    "si": ["one", "other"],
    "sv": ["one", "other"],
    # 3-form, Slavic-style (third slot is "many")
    "pl": ["one", "few", "many"],
    "ru": ["one", "few", "many"],
    "uk": ["one", "few", "many"],
    # 3-form, "other" tail
    "cs": ["one", "few", "other"],
    "ro": ["one", "few", "other"],
    "sr_Latn": ["one", "few", "other"],
    # 6-form
    "ar": ["zero", "one", "two", "few", "many", "other"],
}


def get_form_categories(lang: str, form_count: int) -> list[str]:
    """Return the CLDR category labels for a language's plural-form slots.

    Validates that the static map agrees with the .ts file's actual slot
    count — a mismatch means Qt's plural rule for the language has shifted
    (or the static map is wrong) and the LANG_FORMS entry must be updated.
    """
    if lang not in LANG_FORMS:
        raise KeyError(
            f"LANG_FORMS has no entry for {lang!r}. "
            f"Add one in bin/translations/translate.py "
            f"matching the {form_count} <numerusform/> slots emitted by lupdate."
        )
    expected = LANG_FORMS[lang]
    if len(expected) != form_count:
        raise ValueError(
            f"LANG_FORMS[{lang!r}] has {len(expected)} categories "
            f"({expected}) but the .ts file emits {form_count} slots. "
            f"Update LANG_FORMS to match Qt's current plural rule."
        )
    return expected


def read_guide_instructions(lang: str) -> str:
    """Read general GUIDE.md + optional per-language GUIDE.md."""
    parts = []

    if os.path.isfile(GUIDE_FILE):
        with open(GUIDE_FILE) as f:
            parts.append(f.read().strip())
    else:
        print(f"WARNING: {GUIDE_FILE} not found", file=sys.stderr)

    lang_guide = os.path.join(LANGS_DIR, lang, "GUIDE.md")
    if os.path.isfile(lang_guide):
        with open(lang_guide) as f:
            parts.append(f"\n## Language-specific instructions for {lang}\n")
            parts.append(f.read().strip())

    return "\n\n".join(parts)


def _clean_location(filename: str) -> str:
    """Strip lupdate's '../../' relative prefix for readability in prompts."""
    return re.sub(r"^(?:\.\./)+", "", filename)


def extract_unfinished(ts_path: str) -> tuple[list[dict], int]:
    """Extract all unfinished translation entries from a .ts file.

    Returns (unfinished_list, skipped_numerus_count).
    Each entry dict always has 'source' and 'context'; the optional
    'location', 'comment', and 'extracomment' fields are included when
    present in the .ts file to give the translator more context.
    Numerus (plural) messages are skipped here — they have a different
    output shape (a list of grammatical-number forms instead of a
    single string) and are processed by `extract_unfinished_numerus()`.
    """
    tree = ET.parse(ts_path)
    root = tree.getroot()
    unfinished = []
    skipped_numerus = 0

    # lupdate emits <location filename="..."/> only on the first location
    # of each run of same-file messages; subsequent <location line="+N"/>
    # tags inherit the filename from the previous one. Track the most
    # recent filename so we can attach it to every message, not just the
    # first in each file-run.
    current_filename: str | None = None

    for context_elem in root.findall("context"):
        for message in context_elem.findall("message"):
            # Update current_filename from any <location> with an explicit
            # filename attribute, in document order — do this regardless
            # of whether the message is unfinished, so state stays correct.
            for loc in message.findall("location"):
                fname = loc.get("filename")
                if fname:
                    current_filename = _clean_location(fname)
                    break

            translation = message.find("translation")
            if translation is None or translation.get("type") != "unfinished":
                continue
            # Skip numerus (plural) messages — they need special handling
            if message.get("numerus") == "yes":
                skipped_numerus += 1
                continue
            source = message.findtext("source", "")
            if not source:
                continue

            entry = {
                "source": source,
                "context": context_elem.findtext("name", ""),
            }
            if current_filename:
                entry["location"] = current_filename

            comment = message.findtext("comment")
            if comment:
                entry["comment"] = comment.strip()

            extracomment = message.findtext("extracomment")
            if extracomment:
                entry["extracomment"] = extracomment.strip()

            unfinished.append(entry)

    return unfinished, skipped_numerus


def extract_unfinished_numerus(ts_path: str) -> tuple[list[dict], int]:
    """Extract all unfinished numerus (plural) translation entries from a .ts file.

    Returns (unfinished_list, finished_count).
    Each entry dict has 'source', 'context', and 'form_count' — the number
    of <numerusform/> slots lupdate emitted for the language (CLDR-correct
    by virtue of Qt's own plural-rule table). The optional 'location',
    'comment', and 'extracomment' fields mirror extract_unfinished().
    finished_count covers numerus messages already filled in (e.g.,
    inherited from nheko) — useful for progress reporting.
    """
    tree = ET.parse(ts_path)
    root = tree.getroot()
    unfinished = []
    finished = 0

    current_filename: str | None = None

    for context_elem in root.findall("context"):
        for message in context_elem.findall("message"):
            for loc in message.findall("location"):
                fname = loc.get("filename")
                if fname:
                    current_filename = _clean_location(fname)
                    break

            if message.get("numerus") != "yes":
                continue

            translation = message.find("translation")
            if translation is None:
                continue

            form_count = len(translation.findall("numerusform"))

            if translation.get("type") != "unfinished":
                finished += 1
                continue

            source = message.findtext("source", "")
            if not source:
                continue

            entry = {
                "source": source,
                "context": context_elem.findtext("name", ""),
                "form_count": form_count,
            }
            if current_filename:
                entry["location"] = current_filename

            comment = message.findtext("comment")
            if comment:
                entry["comment"] = comment.strip()

            extracomment = message.findtext("extracomment")
            if extracomment:
                entry["extracomment"] = extracomment.strip()

            unfinished.append(entry)

    return unfinished, finished


def write_ts(ts_path: str, root: ET.Element):
    """Write an ElementTree root back to a .ts file in canonical format.

    Preserves the XML declaration and DOCTYPE from the original file,
    then writes the ElementTree body with consistent formatting:
    - No space before /> in self-closing tags
    - Trailing newline
    """
    # Read original to extract the header
    with open(ts_path, "r", encoding="utf-8") as f:
        original = f.read()

    header_lines = []
    for line in original.split("\n"):
        stripped = line.strip()
        if stripped.startswith("<?xml") or stripped.startswith("<!DOCTYPE"):
            header_lines.append(line)
        else:
            break

    body = ET.tostring(root, encoding="unicode", xml_declaration=False)
    # Normalize self-closing tags (no space before />)
    body = re.sub(r" />", "/>", body)

    with open(ts_path, "w", encoding="utf-8") as f:
        if header_lines:
            f.write("\n".join(header_lines) + "\n")
        f.write(body)
        f.write("\n")


def normalize_ts(ts_path: str) -> bool:
    """Normalize a .ts file to canonical XML format (idempotent).

    Returns True if the file was changed.
    """
    with open(ts_path, "r", encoding="utf-8") as f:
        before = f.read()

    tree = ET.parse(ts_path)
    write_ts(ts_path, tree.getroot())

    with open(ts_path, "r", encoding="utf-8") as f:
        after = f.read()

    return before != after


def inject_translations(ts_path: str, translations: dict[tuple[str, str], str]):
    """Write translations back into the .ts file.

    translations is a dict keyed by (context, source) -> translation string.
    Modifies the file in place using proper XML parsing via ElementTree.
    """
    tree = ET.parse(ts_path)
    root = tree.getroot()

    injected = 0
    for context_elem in root.findall("context"):
        context_name = context_elem.findtext("name", "")
        for message in context_elem.findall("message"):
            translation_elem = message.find("translation")
            if (
                translation_elem is not None
                and translation_elem.get("type") == "unfinished"
            ):
                source = message.findtext("source", "")
                key = (context_name, source)
                if key in translations:
                    translation_elem.text = translations[key]
                    del translation_elem.attrib["type"]
                    injected += 1

    write_ts(ts_path, root)
    return injected


def inject_numerus_translations(
    ts_path: str,
    translations: dict[tuple[str, str], list[str]],
) -> int:
    """Write numerus (plural) translations back into the .ts file.

    translations is keyed by (context, source) -> list of form strings,
    ordered to match the <numerusform/> slots in the .ts file (which are
    themselves in CLDR canonical order). A form-count mismatch between
    model output and slot count is logged and skipped, leaving the
    message unfinished for the next run.
    """
    tree = ET.parse(ts_path)
    root = tree.getroot()

    injected = 0
    for context_elem in root.findall("context"):
        context_name = context_elem.findtext("name", "")
        for message in context_elem.findall("message"):
            if message.get("numerus") != "yes":
                continue
            translation_elem = message.find("translation")
            if (
                translation_elem is None
                or translation_elem.get("type") != "unfinished"
            ):
                continue
            source = message.findtext("source", "")
            key = (context_name, source)
            if key not in translations:
                continue
            forms = translations[key]
            slots = translation_elem.findall("numerusform")
            if len(slots) != len(forms):
                print(
                    f"  SKIPPED [{source!r}]: form count mismatch "
                    f"(slots: {len(slots)}, forms: {len(forms)})",
                    file=sys.stderr,
                )
                continue
            for slot, text in zip(slots, forms):
                slot.text = text
            del translation_elem.attrib["type"]
            injected += 1

    write_ts(ts_path, root)
    return injected


_PLACEHOLDER_RE = re.compile(r"%(?:L?\d|n)")
# Match angle-bracketed tokens whose first letter is any Unicode letter,
# not just ASCII a-zA-Z. CLI-style placeholders like `<target>=<level>`
# legitimately get localized to `<cíl>=<úroveň>` in some languages, and
# an ASCII-only regex would count tag-preservation incorrectly there.
# `\w` in Python 3 is Unicode-aware by default for str patterns.
_HTML_TAG_RE = re.compile(r"</?\w[^>]*/?>")
_SHORTCUT_RE = re.compile(
    r"(?:Ctrl|Alt|Shift|Meta|Cmd|Super)\+"
    r"(?:F\d+|[A-Za-z0-9]+|Enter|Return|Space|Tab|Esc|Escape|"
    r"Delete|Del|Backspace|Insert|Home|End|PageUp|PageDown|"
    r"Left|Right|Up|Down)"
)


def validate_translation(source: str, translation: str) -> list[str]:
    """Return a list of problems found in `translation` relative to `source`.

    Empty list means the translation is structurally sound. Checks:
    - every placeholder (%1, %2, %n, %L1, ...) in source appears at
      least as many times in translation
    - HTML-like tag count matches (we tolerate attribute reordering)
    - the count of '&&' literal ampersand escapes is preserved
    - every keyboard shortcut token (Ctrl+K etc.) in source appears
      verbatim in translation

    Intentionally lenient on anything cosmetic (quote style, whitespace,
    XML entities — those are already decoded by ElementTree before we
    see them) to avoid false positives. The goal is catching breakage
    the user would see at runtime, not style nits.
    """
    problems: list[str] = []

    src_ph = Counter(_PLACEHOLDER_RE.findall(source))
    tr_ph = Counter(_PLACEHOLDER_RE.findall(translation))
    for ph, count in src_ph.items():
        if tr_ph.get(ph, 0) < count:
            problems.append(
                f"placeholder {ph!r} missing "
                f"(source: {count}, translation: {tr_ph.get(ph, 0)})"
            )

    src_tags = len(_HTML_TAG_RE.findall(source))
    tr_tags = len(_HTML_TAG_RE.findall(translation))
    if src_tags != tr_tags:
        problems.append(
            f"HTML tag count mismatch (source: {src_tags}, translation: {tr_tags})"
        )

    src_amp = source.count("&&")
    tr_amp = translation.count("&&")
    if src_amp != tr_amp:
        problems.append(
            f"'&&' count mismatch (source: {src_amp}, translation: {tr_amp})"
        )

    for sc in set(_SHORTCUT_RE.findall(source)):
        if sc not in translation:
            problems.append(f"keyboard shortcut {sc!r} missing from translation")

    return problems


def build_prompt(batch: list[dict], lang: str, instructions: str) -> str:
    """Render the LLM prompt for a batch. Exposed for --print-prompt."""
    lang_name = LANGUAGE_NAMES.get(lang, lang)
    return f"""{instructions}

---

Translate the following UI strings from English to **{lang_name}** ({lang}).

Your response MUST start with `[` and end with `]`. No preamble, no explanation, no markdown fences — just the JSON array. Each element must have "source" (unchanged from input) and "translation" fields. Return exactly one object per input item, in the same order — do not omit any.

Input:
```json
{json.dumps(batch, ensure_ascii=False, indent=2)}
```"""


def build_numerus_prompt(
    batch: list[dict],
    lang: str,
    instructions: str,
    form_categories: list[str],
) -> str:
    """Render the LLM prompt for a batch of numerus (plural) entries."""
    lang_name = LANGUAGE_NAMES.get(lang, lang)
    form_count = len(form_categories)

    if form_count == 1:
        forms_block = (
            f"This language ({lang_name}) uses **a single plural form** "
            "covering all counts (the CLDR \"other\" category). Return one "
            "string per source — typically the same form regardless of count."
        )
    else:
        lines = [
            f"This language ({lang_name}) uses **{form_count} plural forms**, "
            "to be returned in this order (CLDR canonical):"
        ]
        for i, cat in enumerate(form_categories, start=1):
            lines.append(f"  {i}. {cat}")
        forms_block = "\n".join(lines) + (
            "\n\nApply the standard CLDR plural rules for this language. "
            "If the per-language GUIDE above lists specific rules, follow those."
        )

    # form_count is invariant within a batch — already stated in the
    # forms_block header, so stripping it from each entry keeps the
    # JSON noise down without losing information.
    slim_batch = [{k: v for k, v in e.items() if k != "form_count"} for e in batch]

    plural_s = "s" if form_count != 1 else ""
    return f"""{instructions}

---

Translate the following UI strings from English to **{lang_name}** ({lang}). These are **plural-form (numerus) messages** — each source uses `%n` as a count placeholder, and the translation must provide a different form for each grammatical-number category that the language distinguishes.

{forms_block}

Every form MUST:
- Preserve the literal `%n` placeholder.
- Preserve any `%1`, `%2`, ... placeholders, HTML tags, `&&` literals, and keyboard shortcuts from the source verbatim.

Your response MUST start with `[` and end with `]`. No preamble, no explanation, no markdown fences — just the JSON array. Each element must have "source" (unchanged from input) and "forms" (an array of exactly {form_count} string{plural_s}, in the order given above). Return exactly one object per input item, in the same order — do not omit any.

Input:
```json
{json.dumps(slim_batch, ensure_ascii=False, indent=2)}
```"""


def call_claude(prompt: str, model: str | None) -> str:
    """Call the Claude CLI with a prompt and return the response."""
    cmd = [
        "claude",
        "-p",
        "--output-format",
        "text",
        "--no-session-persistence",
    ]
    if model:
        cmd.extend(["--model", model])

    result = subprocess.run(
        cmd,
        input=prompt,
        capture_output=True,
        text=True,
        timeout=300,
    )

    if result.returncode != 0:
        raise RuntimeError(
            f"Claude CLI failed (exit {result.returncode}):\n{result.stderr}"
        )

    return result.stdout.strip()


def _find_json_array(text: str) -> str | None:
    """Bracket-match the outermost JSON array in `text`, respecting string literals.

    Returns the substring '[...]' (no surrounding prose/fences), or None if no
    balanced array is found. Tolerates arbitrary prefix/suffix text and
    embedded backticks — more forgiving than a plain regex.
    """
    start = text.find("[")
    if start < 0:
        return None
    depth = 0
    in_string = False
    escape_next = False
    for i in range(start, len(text)):
        c = text[i]
        if escape_next:
            escape_next = False
            continue
        if in_string:
            if c == "\\":
                escape_next = True
            elif c == '"':
                in_string = False
            continue
        if c == '"':
            in_string = True
            continue
        if c == "[":
            depth += 1
        elif c == "]":
            depth -= 1
            if depth == 0:
                return text[start : i + 1]
    return None


def _is_regular_shape(obj: dict) -> bool:
    """Predicate: object is a regular {source, translation} pair."""
    return (
        isinstance(obj.get("source"), str)
        and isinstance(obj.get("translation"), str)
    )


def _is_numerus_shape(obj: dict) -> bool:
    """Predicate: object is a numerus {source, forms: [str, ...]} pair."""
    forms = obj.get("forms")
    return (
        isinstance(obj.get("source"), str)
        and isinstance(forms, list)
        and all(isinstance(f, str) for f in forms)
    )


def _recover_entries(text: str, shape_check=_is_regular_shape) -> list[dict]:
    """Best-effort entry recovery from a malformed JSON response.

    Walks the text forward, attempting raw_decode from each '{' position.
    Keeps every well-formed object whose shape matches `shape_check`;
    silently drops any entry that doesn't parse. Useful when the outer
    array parse fails because a single entry is corrupted (e.g. mixed
    typographic and ASCII quotes inside one translation string) — the
    rest of the batch is still salvageable, no Claude call wasted.
    """
    decoder = json.JSONDecoder()
    entries: list[dict] = []
    i = 0
    while i < len(text):
        j = text.find("{", i)
        if j < 0:
            break
        try:
            obj, consumed = decoder.raw_decode(text[j:])
        except json.JSONDecodeError:
            i = j + 1
            continue
        if isinstance(obj, dict) and shape_check(obj):
            entries.append(obj)
        i = j + consumed
    return entries


def extract_json_from_response(
    response: str,
    shape_check=_is_regular_shape,
) -> list[dict]:
    """Extract JSON array from the LLM's response, handling prose and fences.

    Tries, in order: direct parse, markdown code fence, bracket-matched
    array scan, and finally per-entry salvage. The last fallback returns
    whatever well-formed objects can be recovered one at a time — so a
    single corrupted entry only loses itself, not the whole batch. The
    shape_check predicate only affects the salvage fallback; the array
    paths return whatever was parsed verbatim and rely on per-item
    validation downstream.
    """
    try:
        return json.loads(response)
    except json.JSONDecodeError:
        pass

    fence_match = re.search(r"```(?:json)?\s*\n(.*?)\n```", response, re.DOTALL)
    if fence_match:
        try:
            return json.loads(fence_match.group(1))
        except json.JSONDecodeError:
            pass

    array_text = _find_json_array(response)
    if array_text:
        try:
            return json.loads(array_text)
        except json.JSONDecodeError:
            pass

    recovered = _recover_entries(response, shape_check)
    if recovered:
        print(
            f"  NOTE: strict JSON parse failed; recovered {len(recovered)} "
            "entries via per-entry salvage",
            file=sys.stderr,
        )
        return recovered

    raise ValueError(f"Could not extract JSON from LLM response:\n{response[:500]}")


def translate_batch(
    batch: list[dict],
    lang: str,
    instructions: str,
    model: str | None,
) -> dict[tuple[str, str], str]:
    """Translate a batch of strings using the configured LLM.

    Returns dict of (context, source) -> translation. Translations that
    fail validation (missing placeholders, dropped HTML tags, etc.) are
    excluded so they remain 'unfinished' and get re-processed on the
    next run.
    """
    prompt = build_prompt(batch, lang, instructions)
    response = call_claude(prompt, model)
    results = extract_json_from_response(response)

    # Build lookup: find all contexts for each source from the batch.
    # The same source string can appear in multiple contexts.
    source_to_contexts: dict[str, list[str]] = {}
    for item in batch:
        source_to_contexts.setdefault(item["source"], []).append(item["context"])

    returned_sources: set[str] = set()
    translations: dict[tuple[str, str], str] = {}
    rejected = 0

    for item in results:
        source = item.get("source", "")
        translation = item.get("translation", "")
        if not (source and translation):
            continue
        returned_sources.add(source)

        problems = validate_translation(source, translation)
        if problems:
            rejected += 1
            print(
                f"  REJECTED [{source!r}]: {'; '.join(problems)}",
                file=sys.stderr,
            )
            print(f"    translation was: {translation!r}", file=sys.stderr)
            continue

        for context in source_to_contexts.get(source, [""]):
            translations[(context, source)] = translation

    missing = [item["source"] for item in batch if item["source"] not in returned_sources]
    if missing:
        print(f"  SKIPPED by model ({len(missing)}):", file=sys.stderr)
        for source in missing[:10]:
            print(f"    - {source!r}", file=sys.stderr)
        if len(missing) > 10:
            print(f"    ... and {len(missing) - 10} more", file=sys.stderr)

    if rejected:
        print(f"  Rejected {rejected} translations by validator", file=sys.stderr)

    return translations


def translate_batch_numerus(
    batch: list[dict],
    lang: str,
    instructions: str,
    model: str | None,
) -> dict[tuple[str, str], list[str]]:
    """Translate a batch of numerus (plural) strings using the configured LLM.

    Returns dict of (context, source) -> list[str] (one string per
    grammatical-number category, in CLDR canonical order). Entries that
    fail validation (wrong form count, missing %n, dropped HTML tags,
    etc.) are excluded so they remain 'unfinished' and get re-processed
    on the next run.

    All entries in `batch` MUST share the same form_count — the caller
    is responsible for grouping by language (which is the natural unit
    here anyway, since one .ts file produces one batch).
    """
    if not batch:
        return {}

    form_count = batch[0]["form_count"]
    form_categories = get_form_categories(lang, form_count)

    prompt = build_numerus_prompt(batch, lang, instructions, form_categories)
    response = call_claude(prompt, model)
    results = extract_json_from_response(response, shape_check=_is_numerus_shape)

    source_to_contexts: dict[str, list[str]] = {}
    for item in batch:
        source_to_contexts.setdefault(item["source"], []).append(item["context"])

    returned_sources: set[str] = set()
    translations: dict[tuple[str, str], list[str]] = {}
    rejected = 0

    for item in results:
        source = item.get("source", "")
        forms = item.get("forms", [])
        if not (source and isinstance(forms, list)):
            continue
        returned_sources.add(source)

        if len(forms) != form_count:
            rejected += 1
            print(
                f"  REJECTED [{source!r}]: form count {len(forms)}, "
                f"expected {form_count}",
                file=sys.stderr,
            )
            continue

        per_form_problems: list[tuple[int, list[str]]] = []
        for i, form in enumerate(forms):
            problems = validate_translation(source, form)
            if problems:
                per_form_problems.append((i, problems))
        if per_form_problems:
            rejected += 1
            for i, probs in per_form_problems:
                print(
                    f"  REJECTED [{source!r}] form {i + 1} "
                    f"({form_categories[i]}): {'; '.join(probs)}",
                    file=sys.stderr,
                )
                print(f"    form was: {forms[i]!r}", file=sys.stderr)
            continue

        for context in source_to_contexts.get(source, [""]):
            translations[(context, source)] = list(forms)

    missing = [item["source"] for item in batch if item["source"] not in returned_sources]
    if missing:
        print(f"  SKIPPED by model ({len(missing)}):", file=sys.stderr)
        for source in missing[:10]:
            print(f"    - {source!r}", file=sys.stderr)
        if len(missing) > 10:
            print(f"    ... and {len(missing) - 10} more", file=sys.stderr)

    if rejected:
        print(f"  Rejected {rejected} numerus translations by validator", file=sys.stderr)

    return translations


def cmd_normalize(args):
    """Normalize .ts files to canonical XML format."""
    if args.lang:
        ts_path = os.path.join(LANGS_DIR, args.lang, f"komai_{args.lang}.ts")
        if not os.path.isfile(ts_path):
            print(f"ERROR: {ts_path} not found", file=sys.stderr)
            sys.exit(1)
        ts_files = [ts_path]
    else:
        ts_files = sorted(glob.glob(os.path.join(LANGS_DIR, "*", "komai_*.ts")))

    changed = 0
    for ts_path in ts_files:
        lang = os.path.basename(os.path.dirname(ts_path))
        if normalize_ts(ts_path):
            print(f"  Normalized: {lang}")
            changed += 1

    if changed:
        print(f"Normalized {changed} file(s)")
    else:
        print("All files already normalized")


def _print_dry_run_samples(label: str, entries: list[dict]):
    """Show the first 10 entries of a dry-run pass."""
    print(f"\nDry run — first 10 {label}:")
    for item in entries[:10]:
        extras = []
        if item.get("location"):
            extras.append(f"loc={item['location']}")
        if item.get("extracomment"):
            extras.append(f"hint={item['extracomment']!r}")
        if item.get("form_count") is not None:
            extras.append(f"forms={item['form_count']}")
        suffix = f" ({', '.join(extras)})" if extras else ""
        print(f"  [{item['context']}] {item['source']}{suffix}")
    if len(entries) > 10:
        print(f"  ... and {len(entries) - 10} more")


def _run_translation_pass(
    label: str,
    entries: list[dict],
    batch_size: int,
    translate_fn,
    inject_fn,
    ts_path: str,
    lang: str,
    instructions: str,
    model: str | None,
) -> tuple[int, list[tuple[int, str]]]:
    """Run a translation pass: batch -> translate -> inject -> repeat.

    Shared between regular and numerus paths. translate_fn and inject_fn
    are the two flavors injected by the caller. Returns
    (total_injected, failed_batches).
    """
    total_batches = (len(entries) + batch_size - 1) // batch_size
    total_injected = 0
    failed_batches: list[tuple[int, str]] = []

    for batch_idx in range(total_batches):
        start = batch_idx * batch_size
        end = min(start + batch_size, len(entries))
        batch = entries[start:end]

        print(
            f"\n[{label} {batch_idx + 1}/{total_batches}] "
            f"Translating items {start + 1}-{end} of {len(entries)}..."
        )

        try:
            translations = translate_fn(batch, lang, instructions, model)
        except (RuntimeError, ValueError, subprocess.TimeoutExpired) as e:
            reason = str(e).splitlines()[0][:120] if str(e) else type(e).__name__
            print(f"  ERROR: {e}", file=sys.stderr)
            print(
                "  Skipping batch — items stay unfinished and will be "
                "retried on the next run.",
                file=sys.stderr,
            )
            failed_batches.append((batch_idx + 1, reason))
            continue

        received = len(translations)
        expected = len(batch)
        if received < expected:
            print(
                f"  WARNING: received {received}/{expected} translations "
                f"(some may have been skipped by the model)"
            )

        injected = inject_fn(ts_path, translations)
        total_injected += injected
        print(f"  Injected {injected} translations (total: {total_injected})")

    return total_injected, failed_batches


def cmd_translate(args):
    """Translate unfinished strings for a language using the configured LLM."""
    ts_path = os.path.join(LANGS_DIR, args.lang, f"komai_{args.lang}.ts")
    if not os.path.isfile(ts_path):
        print(f"ERROR: {ts_path} not found", file=sys.stderr)
        sys.exit(1)

    do_regular = not args.numerus_only
    do_numerus = not args.regular_only

    unfinished, _ = extract_unfinished(ts_path)
    numerus_unfinished, numerus_finished = extract_unfinished_numerus(ts_path)

    # Cluster by (context, source) so related strings stay in the same
    # batch — gives the model consistency pressure within one call.
    unfinished.sort(key=lambda e: (e.get("context", ""), e["source"]))
    numerus_unfinished.sort(key=lambda e: (e.get("context", ""), e["source"]))

    regular_pending = unfinished if do_regular else []
    numerus_pending = numerus_unfinished if do_numerus else []

    if not regular_pending and not numerus_pending:
        print(f"No unfinished translations in komai_{args.lang}.ts")
        if not do_regular and unfinished:
            print(f"  ({len(unfinished)} regular strings skipped — --numerus-only)")
        if not do_numerus and numerus_unfinished:
            print(f"  ({len(numerus_unfinished)} plural forms skipped — --regular-only)")
        return

    lang_name = LANGUAGE_NAMES.get(args.lang, args.lang)
    print(f"Language: {lang_name} ({args.lang})")
    if do_regular:
        print(f"Regular unfinished: {len(unfinished)} strings")
    if do_numerus:
        cats = LANG_FORMS.get(args.lang)
        cats_repr = "/".join(cats) if cats else "?"
        print(
            f"Numerus unfinished: {len(numerus_unfinished)} messages "
            f"({cats_repr})"
        )
    print(f"Batch size: {args.batch_size}")

    if args.dry_run:
        if regular_pending:
            _print_dry_run_samples("regular unfinished strings", regular_pending)
        if numerus_pending:
            _print_dry_run_samples("numerus unfinished messages", numerus_pending)
        return

    instructions = read_guide_instructions(args.lang)

    if args.print_prompt:
        # Default to numerus prompt only if regular pass is skipped or
        # has nothing to do — otherwise show the regular one. Lets the
        # user opt into the numerus prompt with --numerus-only.
        if numerus_pending and (not regular_pending or args.numerus_only):
            cats = get_form_categories(args.lang, numerus_pending[0]["form_count"])
            first_batch = numerus_pending[: args.batch_size]
            print(build_numerus_prompt(first_batch, args.lang, instructions, cats))
        else:
            first_batch = regular_pending[: args.batch_size]
            print(build_prompt(first_batch, args.lang, instructions))
        return

    # Process in batches. Individual batch failures (model returning
    # unparseable output, transient CLI errors) don't abort the run —
    # those strings stay 'unfinished' and get retried on the next
    # invocation.
    grand_injected = 0
    all_failed: list[tuple[str, int, str]] = []

    if regular_pending:
        injected, failed = _run_translation_pass(
            label="regular",
            entries=regular_pending,
            batch_size=args.batch_size,
            translate_fn=translate_batch,
            inject_fn=inject_translations,
            ts_path=ts_path,
            lang=args.lang,
            instructions=instructions,
            model=args.model,
        )
        grand_injected += injected
        all_failed.extend(("regular", idx, reason) for idx, reason in failed)

    if numerus_pending:
        injected, failed = _run_translation_pass(
            label="numerus",
            entries=numerus_pending,
            batch_size=args.batch_size,
            translate_fn=translate_batch_numerus,
            inject_fn=inject_numerus_translations,
            ts_path=ts_path,
            lang=args.lang,
            instructions=instructions,
            model=args.model,
        )
        grand_injected += injected
        all_failed.extend(("numerus", idx, reason) for idx, reason in failed)

    print(f"\nDone. {grand_injected} translations written to komai_{args.lang}.ts")

    if all_failed:
        print(
            f"WARNING: {len(all_failed)} batch(es) failed and were skipped — "
            "re-run the command to retry them:",
            file=sys.stderr,
        )
        for label, idx, reason in all_failed:
            print(f"  {label} batch {idx}: {reason}", file=sys.stderr)

    remaining, _ = extract_unfinished(ts_path)
    remaining_numerus, _ = extract_unfinished_numerus(ts_path)
    if remaining:
        print(f"Remaining regular unfinished: {len(remaining)} strings")
    if remaining_numerus:
        print(f"Remaining numerus unfinished: {len(remaining_numerus)} messages")
    if not remaining and not remaining_numerus:
        print("All strings are now translated!")


def main():
    parser = argparse.ArgumentParser(
        description="Manage Komai translations: normalize and auto-translate"
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    # normalize subcommand
    norm_parser = subparsers.add_parser(
        "normalize", help="Normalize .ts files to canonical XML format"
    )
    norm_parser.add_argument(
        "--lang",
        default=None,
        help="Language code to normalize (default: all languages)",
    )

    # translate subcommand
    trans_parser = subparsers.add_parser(
        "translate", help="Translate unfinished strings using an LLM"
    )
    trans_parser.add_argument("lang", help="Language code (e.g., de, fr, ja)")
    trans_parser.add_argument(
        "--batch-size",
        type=int,
        default=75,
        help="Number of strings per LLM call (default: 75)",
    )
    trans_parser.add_argument(
        "--model",
        default="sonnet",
        help=(
            "Model to use (default: sonnet). Pass 'opus' or a full model "
            "ID to override. Sonnet is the default because translation is "
            "a high-volume structured-output task — Opus is slower and "
            "its extra reasoning adds no quality here, while Sonnet is "
            "~3-5x faster and equally reliable at JSON framing."
        ),
    )
    trans_parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Extract and show unfinished strings without translating",
    )
    trans_parser.add_argument(
        "--print-prompt",
        action="store_true",
        help="Print the prompt for the first batch and exit (debug aid)",
    )
    pass_group = trans_parser.add_mutually_exclusive_group()
    pass_group.add_argument(
        "--regular-only",
        action="store_true",
        help="Translate only regular strings; skip plural-form (numerus) messages.",
    )
    pass_group.add_argument(
        "--numerus-only",
        action="store_true",
        help="Translate only plural-form (numerus) messages; skip regular strings.",
    )

    args = parser.parse_args()

    if args.command == "normalize":
        cmd_normalize(args)
    elif args.command == "translate":
        cmd_translate(args)


if __name__ == "__main__":
    main()
