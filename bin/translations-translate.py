#!/usr/bin/env python3
"""Translate unfinished Qt .ts strings using the Claude CLI.

Usage:
    python3 bin/translations-translate.py <lang> [--batch-size N] [--model MODEL] [--dry-run]

Examples:
    python3 bin/translations-translate.py de
    python3 bin/translations-translate.py ja --batch-size 50
    python3 bin/translations-translate.py fr --dry-run

The script:
1. Parses resources/langs/<lang>/komai_<lang>.ts for unfinished translations
2. Sends batches of source strings to the Claude CLI for translation
3. Injects each batch back into the .ts file immediately (incremental save)
4. On re-run, only processes remaining unfinished strings

Requires the `claude` CLI to be installed and authenticated.
"""

import argparse
import json
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.join(SCRIPT_DIR, "..")
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


def extract_unfinished(ts_path: str) -> list[dict]:
    """Extract all unfinished translation entries from a .ts file."""
    tree = ET.parse(ts_path)
    root = tree.getroot()
    unfinished = []

    for context_elem in root.findall("context"):
        context_name = context_elem.findtext("name", "")
        for message in context_elem.findall("message"):
            translation = message.find("translation")
            if translation is not None and translation.get("type") == "unfinished":
                source = message.findtext("source", "")
                if source:
                    unfinished.append(
                        {
                            "source": source,
                            "context": context_name,
                        }
                    )

    return unfinished


def inject_translations(ts_path: str, translations: dict[tuple[str, str], str]):
    """Write translations back into the .ts file.

    translations is a dict keyed by (context, source) -> translation string.
    Modifies the file in place, preserving the original XML structure.

    We use regex-based replacement instead of ElementTree to preserve
    the original formatting, comments, and whitespace of the .ts file.
    """
    with open(ts_path, "r", encoding="utf-8") as f:
        content = f.read()

    injected = 0

    # Process each context/message block
    # We need to track which context we're in
    current_context = None

    def replace_in_context(match):
        nonlocal current_context
        current_context = match.group(1)
        return match.group(0)

    # First pass: find context names
    # Second pass: replace unfinished translations within each context

    # Strategy: iterate through the file, tracking context, and replace
    # unfinished translations where we have a match.

    lines = content.split("\n")
    output_lines = []
    current_context = None
    current_source = None
    current_source_unescaped = None
    i = 0

    while i < len(lines):
        line = lines[i]

        # Track context
        context_match = re.match(r"\s*<name>(.*?)</name>", line)
        if context_match:
            current_context = context_match.group(1)

        # Track source (may span multiple lines, but typically single)
        source_match = re.match(r"\s*<source>(.*?)</source>", line)
        if source_match:
            current_source = source_match.group(1)
            # Unescape XML entities for lookup
            current_source_unescaped = (
                current_source.replace("&amp;", "&")
                .replace("&lt;", "<")
                .replace("&gt;", ">")
                .replace("&quot;", '"')
                .replace("&apos;", "'")
            )

        # Check for unfinished translation line
        unfinished_match = re.match(
            r'(\s*)<translation type="unfinished">(.*?)</translation>', line
        )
        if (
            unfinished_match
            and current_context
            and current_source
            and current_source_unescaped
        ):
            key = (current_context, current_source_unescaped)
            if key in translations:
                indent = unfinished_match.group(1)
                new_text = translations[key]
                # Escape for XML
                new_text_escaped = (
                    new_text.replace("&", "&amp;")
                    .replace("<", "&lt;")
                    .replace(">", "&gt;")
                    .replace('"', "&quot;")
                    .replace("'", "&apos;")
                )
                output_lines.append(
                    f"{indent}<translation>{new_text_escaped}</translation>"
                )
                injected += 1
                i += 1
                continue

        output_lines.append(line)
        i += 1

    with open(ts_path, "w", encoding="utf-8") as f:
        f.write("\n".join(output_lines))

    return injected


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
    """Extract JSON array from Claude's response, handling markdown fences."""
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

    raise ValueError(f"Could not extract JSON from Claude response:\n{response[:500]}")


def translate_batch(
    batch: list[dict],
    lang: str,
    instructions: str,
    model: str | None,
) -> dict[tuple[str, str], str]:
    """Translate a batch of strings using Claude.

    Returns dict of (context, source) -> translation.
    """
    lang_name = LANGUAGE_NAMES.get(lang, lang)

    prompt = f"""{instructions}

---

Translate the following UI strings from English to **{lang_name}** ({lang}).

Return ONLY a JSON array. Each element must have "source" (unchanged) and "translation" fields.

```json
{json.dumps(batch, ensure_ascii=False, indent=2)}
```"""

    response = call_claude(prompt, model)
    results = extract_json_from_response(response)

    # Build lookup: find the context for each source from the batch
    source_to_context = {}
    for item in batch:
        source_to_context[item["source"]] = item["context"]

    translations = {}
    for item in results:
        source = item.get("source", "")
        translation = item.get("translation", "")
        if source and translation:
            context = source_to_context.get(source, "")
            translations[(context, source)] = translation

    return translations


def main():
    parser = argparse.ArgumentParser(
        description="Translate unfinished Qt .ts strings using Claude CLI"
    )
    parser.add_argument("lang", help="Language code (e.g., de, fr, ja)")
    parser.add_argument(
        "--batch-size",
        type=int,
        default=75,
        help="Number of strings per Claude call (default: 75)",
    )
    parser.add_argument(
        "--model",
        default=None,
        help="Claude model to use (default: CLI default)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Extract and show unfinished strings without translating",
    )
    args = parser.parse_args()

    ts_path = os.path.join(LANGS_DIR, args.lang, f"komai_{args.lang}.ts")
    if not os.path.isfile(ts_path):
        print(f"ERROR: {ts_path} not found", file=sys.stderr)
        sys.exit(1)

    # Extract unfinished strings
    unfinished = extract_unfinished(ts_path)

    if not unfinished:
        print(f"No unfinished translations in komai_{args.lang}.ts")
        return

    lang_name = LANGUAGE_NAMES.get(args.lang, args.lang)
    print(f"Language: {lang_name} ({args.lang})")
    print(f"Unfinished: {len(unfinished)} strings")
    print(f"Batch size: {args.batch_size}")

    if args.dry_run:
        print(f"\nDry run — first 10 unfinished strings:")
        for item in unfinished[:10]:
            print(f"  [{item['context']}] {item['source']}")
        if len(unfinished) > 10:
            print(f"  ... and {len(unfinished) - 10} more")
        return

    # Read agent instructions
    instructions = read_guide_instructions(args.lang)

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
                f"(some may have been skipped by Claude)"
            )

        # Inject immediately
        injected = inject_translations(ts_path, translations)
        total_injected += injected
        print(f"  Injected {injected} translations (total: {total_injected})")

    print(f"\nDone. {total_injected} translations written to komai_{args.lang}.ts")

    # Report remaining
    remaining = extract_unfinished(ts_path)
    if remaining:
        print(f"Remaining unfinished: {remaining}")
    else:
        print("All strings are now translated!")


if __name__ == "__main__":
    main()
