#!/usr/bin/env python3
"""Pick a canonical translation per source string and propagate it within each .ts file.

Background: when a `.ts` file has the same English `<source>` translated
multiple distinct ways across different `<context>` blocks, the UI
shows inconsistent terminology (e.g., "Remove" rendered as both
"Odebrat" and "Odstranit" in Czech depending on which dialog you're in).
Each individual translation is grammatically correct, but the mix is
jarring.

This script picks one canonical translation per source string and
overwrites the others. Strategy:

1. Most-frequent wins (commonest translation across the file is the
   most representative).
2. Tie-break by per-language GUIDE preference if codified in
   GUIDE_PREFS below (e.g., German prefers "Nutzer" over "Benutzer").
3. Final tie-break: lexicographic order, for determinism.

Usage:
    python3 bin/translations/normalize-inconsistencies.py            # all languages
    python3 bin/translations/normalize-inconsistencies.py --lang de  # one language
    python3 bin/translations/normalize-inconsistencies.py --dry-run  # report only

Numerus messages and unfinished entries are skipped.
"""
import argparse
import glob
import os
import re
import sys
import xml.etree.ElementTree as ET
from collections import Counter, defaultdict

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
PROJECT_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
LANGS_DIR = os.path.join(PROJECT_ROOT, "resources", "langs")

# Per-language GUIDE-codified preferences. Substring match: if any
# tied-canonical candidate contains one of these, that candidate wins.
GUIDE_PREFS: dict[str, list[str]] = {
    "de": ["Nutzer"],            # "Nutzer" preferred over "Benutzer"
    "fr": ["salon"],             # "salon" preferred over alternatives
    "ca": ["sala", "sales"],
    "pt_PT": ["utilizador", "encriptaç"],   # vs the Brazilian forms
    "pt_BR": ["usuário", "criptograf"],     # vs the European forms
}


def write_ts(ts_path: str, root: ET.Element) -> None:
    """Reproduce write_ts() from translate.py to keep canonical formatting."""
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
    body = re.sub(r" />", "/>", body)
    with open(ts_path, "w", encoding="utf-8") as f:
        if header_lines:
            f.write("\n".join(header_lines) + "\n")
        f.write(body)
        f.write("\n")


def normalize_lang(ts_path: str, lang: str, dry_run: bool) -> list[tuple[str, str, str, str]]:
    tree = ET.parse(ts_path)
    root = tree.getroot()

    # (source, extracomment) -> [(context, translation_elem, current_text)]
    # Entries with an <extracomment> are explicitly disambiguated by the
    # developers (e.g., a bare keycap label vs. the same word as a noun).
    # Group them separately so canonicalisation never collapses across the
    # disambiguation boundary.
    source_translations: dict[tuple[str, str], list[tuple[str, ET.Element, str]]] = defaultdict(list)
    for ctx in root.findall("context"):
        cname = ctx.findtext("name", "")
        for msg in ctx.findall("message"):
            t = msg.find("translation")
            if t is None or t.get("type") == "unfinished" or msg.get("numerus") == "yes":
                continue
            source = msg.findtext("source", "")
            tr = (t.text or "").strip()
            if not (source and tr):
                continue
            extra = (msg.findtext("extracomment", "") or "").strip()
            source_translations[(source, extra)].append((cname, t, tr))

    prefs = GUIDE_PREFS.get(lang, [])
    changed: list[tuple[str, str, str, str]] = []

    for (source, _extra), entries in source_translations.items():
        translations = [e[2] for e in entries]
        if len(set(translations)) <= 1:
            continue

        counter = Counter(translations)
        max_count = max(counter.values())
        candidates = [t for t, c in counter.items() if c == max_count]
        if len(candidates) > 1:
            for pref in prefs:
                preferred = [c for c in candidates if pref in c]
                if preferred:
                    candidates = preferred
                    break
            if len(candidates) > 1:
                candidates.sort()
        canonical = candidates[0]

        for cname, t_elem, tr in entries:
            if tr != canonical:
                if not dry_run:
                    t_elem.text = canonical
                changed.append((source, cname, tr, canonical))

    if changed and not dry_run:
        write_ts(ts_path, root)
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--lang", default=None, help="Single language to normalize (default: all)")
    parser.add_argument("--dry-run", action="store_true", help="Report only, do not modify files")
    args = parser.parse_args()

    if args.lang:
        ts_files = [os.path.join(LANGS_DIR, args.lang, f"komai_{args.lang}.ts")]
        if not os.path.isfile(ts_files[0]):
            print(f"ERROR: {ts_files[0]} not found", file=sys.stderr)
            return 1
    else:
        ts_files = sorted(glob.glob(os.path.join(LANGS_DIR, "*", "komai_*.ts")))

    total_changed = 0
    for ts_path in ts_files:
        lang = os.path.basename(os.path.dirname(ts_path))
        if lang == "en":
            continue
        changes = normalize_lang(ts_path, lang, args.dry_run)
        total_changed += len(changes)
        action = "would canonicalize" if args.dry_run else "canonicalized"
        if changes:
            print(f"\n=== {lang}: {action} {len(changes)} translations ===")
            for source, ctx, was, now in changes[:5]:
                print(f"  [{ctx}] {source[:50]!r}")
                print(f"      was: {was[:60]!r}")
                print(f"      now: {now[:60]!r}")
            if len(changes) > 5:
                print(f"  ... and {len(changes)-5} more")

    print(f"\nTotal: {total_changed} translations {'previewed' if args.dry_run else 'canonicalized'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
