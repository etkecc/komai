#!/usr/bin/env python3
"""Check that staged .ts translation files are in normalized XML form.

Compares each staged .ts file against what our normalize function would
produce. Fails if any file would change, indicating it was not normalized
(e.g., lupdate was run without `just translations-update`).
"""

import glob
import os
import re
import subprocess
import sys
import xml.etree.ElementTree as ET


def get_staged_ts_files(langs_dir: str) -> list[str]:
    """Return .ts files under langs_dir that are staged in git."""
    result = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACMR"],
        capture_output=True,
        text=True,
    )
    staged = result.stdout.strip().split("\n") if result.stdout.strip() else []

    ts_files = []
    for path in staged:
        abs_path = os.path.abspath(path)
        if abs_path.startswith(os.path.abspath(langs_dir)) and abs_path.endswith(".ts"):
            ts_files.append(abs_path)
    return ts_files


def would_normalize_change(ts_path: str) -> bool:
    """Return True if normalizing the file would change it."""
    with open(ts_path, "r", encoding="utf-8") as f:
        before = f.read()

    try:
        tree = ET.parse(ts_path)
    except ET.ParseError:
        # Can't parse — let it through, other tools will catch XML errors
        return False

    root = tree.getroot()

    # Reproduce the same normalization as write_ts in translations-translate.py
    header_lines = []
    for line in before.split("\n"):
        stripped = line.strip()
        if stripped.startswith("<?xml") or stripped.startswith("<!DOCTYPE"):
            header_lines.append(line)
        else:
            break

    body = ET.tostring(root, encoding="unicode", xml_declaration=False)
    body = re.sub(r" />", "/>", body)

    after = ""
    if header_lines:
        after += "\n".join(header_lines) + "\n"
    after += body + "\n"

    return before != after


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.join(script_dir, "..")
    langs_dir = os.path.join(project_root, "resources", "langs")

    # Check staged files if any, otherwise check all
    staged = get_staged_ts_files(langs_dir)
    if staged:
        ts_files = staged
    else:
        ts_files = sorted(glob.glob(os.path.join(langs_dir, "*", "komai_*.ts")))

    if not ts_files:
        print("No .ts files to check.")
        sys.exit(0)

    not_normalized = []
    for ts_path in ts_files:
        lang = os.path.basename(os.path.dirname(ts_path))
        if would_normalize_change(ts_path):
            not_normalized.append(lang)

    if not_normalized:
        print("ERROR: Translation files are not normalized!")
        print("")
        for lang in not_normalized:
            print(f"  {lang}")
        print("")
        print("Run 'just translations-normalize' to fix, then re-stage.")
        sys.exit(1)

    print(f"All {len(ts_files)} translation file(s) are normalized.")


if __name__ == "__main__":
    main()
