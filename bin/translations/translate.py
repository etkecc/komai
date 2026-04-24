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
    Numerus (plural) messages are skipped because they require special
    handling with multiple plural forms that varies by language.
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


_PLACEHOLDER_RE = re.compile(r"%(?:L?\d|n)")
_HTML_TAG_RE = re.compile(r"</?[a-zA-Z][^>]*/?>")
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

Return ONLY a JSON array. Each element must have "source" (unchanged) and "translation" fields. Return exactly one object per input item, in the same order — do not omit any.

```json
{json.dumps(batch, ensure_ascii=False, indent=2)}
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


def extract_json_from_response(response: str) -> list[dict]:
    """Extract JSON array from the LLM's response, handling markdown fences."""
    # Try direct parse first
    try:
        return json.loads(response)
    except json.JSONDecodeError:
        pass

    # Try extracting from markdown code fence
    fence_match = re.search(r"```(?:json)?\s*\n(.*?)\n```", response, re.DOTALL)
    if fence_match:
        try:
            return json.loads(fence_match.group(1))
        except json.JSONDecodeError:
            pass

    # Try finding the array directly
    bracket_match = re.search(r"\[.*\]", response, re.DOTALL)
    if bracket_match:
        try:
            return json.loads(bracket_match.group(0))
        except json.JSONDecodeError:
            pass

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


def cmd_translate(args):
    """Translate unfinished strings for a language using the configured LLM."""
    ts_path = os.path.join(LANGS_DIR, args.lang, f"komai_{args.lang}.ts")
    if not os.path.isfile(ts_path):
        print(f"ERROR: {ts_path} not found", file=sys.stderr)
        sys.exit(1)

    # Extract unfinished strings
    unfinished, skipped_numerus = extract_unfinished(ts_path)

    if not unfinished:
        print(f"No unfinished translations in komai_{args.lang}.ts")
        if skipped_numerus:
            print(f"  ({skipped_numerus} plural forms skipped — not yet supported)")
        return

    # Cluster by (context, source) so related strings stay in the same
    # batch — gives the model consistency pressure within one call.
    unfinished.sort(key=lambda e: (e.get("context", ""), e["source"]))

    lang_name = LANGUAGE_NAMES.get(args.lang, args.lang)
    print(f"Language: {lang_name} ({args.lang})")
    print(f"Unfinished: {len(unfinished)} strings")
    if skipped_numerus:
        print(f"Skipped: {skipped_numerus} plural forms (not yet supported)")
    print(f"Batch size: {args.batch_size}")

    if args.dry_run:
        print(f"\nDry run — first 10 unfinished strings:")
        for item in unfinished[:10]:
            extras = []
            if item.get("location"):
                extras.append(f"loc={item['location']}")
            if item.get("extracomment"):
                extras.append(f"hint={item['extracomment']!r}")
            suffix = f" ({', '.join(extras)})" if extras else ""
            print(f"  [{item['context']}] {item['source']}{suffix}")
        if len(unfinished) > 10:
            print(f"  ... and {len(unfinished) - 10} more")
        return

    # Read agent instructions
    instructions = read_guide_instructions(args.lang)

    if args.print_prompt:
        first_batch = unfinished[: args.batch_size]
        print(build_prompt(first_batch, args.lang, instructions))
        return

    # Process in batches
    total_batches = (len(unfinished) + args.batch_size - 1) // args.batch_size
    total_injected = 0

    for batch_idx in range(total_batches):
        start = batch_idx * args.batch_size
        end = min(start + args.batch_size, len(unfinished))
        batch = unfinished[start:end]

        print(
            f"\n[{batch_idx + 1}/{total_batches}] "
            f"Translating strings {start + 1}-{end} of {len(unfinished)}..."
        )

        try:
            translations = translate_batch(batch, args.lang, instructions, args.model)
        except (RuntimeError, ValueError, subprocess.TimeoutExpired) as e:
            print(f"  ERROR: {e}", file=sys.stderr)
            print(
                f"  Stopping. {total_injected} strings saved so far.", file=sys.stderr
            )
            sys.exit(1)

        received = len(translations)
        expected = len(batch)
        if received < expected:
            print(
                f"  WARNING: received {received}/{expected} translations "
                f"(some may have been skipped by the model)"
            )

        # Inject immediately
        injected = inject_translations(ts_path, translations)
        total_injected += injected
        print(f"  Injected {injected} translations (total: {total_injected})")

    print(f"\nDone. {total_injected} translations written to komai_{args.lang}.ts")

    # Report remaining
    remaining, remaining_numerus = extract_unfinished(ts_path)
    if remaining:
        print(f"Remaining unfinished: {len(remaining)} strings")
    else:
        print("All strings are now translated!")
    if remaining_numerus:
        print(f"Remaining plural forms: {remaining_numerus} (not yet supported)")


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
        default=None,
        help="Model to use (default: CLI default)",
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

    args = parser.parse_args()

    if args.command == "normalize":
        cmd_normalize(args)
    elif args.command == "translate":
        cmd_translate(args)


if __name__ == "__main__":
    main()
