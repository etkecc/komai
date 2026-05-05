#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Verify translations are complete enough to ship a release.

Pre-commit hook: when VERSION changes (the canonical signal that a
release is being cut), fail if any non-English translation file
contains ``<translation type="unfinished">`` entries. The English
source file is exempt — its ``<translation>`` elements are placeholders
that resolve to the source string at runtime.

Day-to-day translation work may legitimately leave some strings
unfinished for a while; this hook only enforces completeness at
release-cut time so we don't ship those gaps to users.
"""

from __future__ import annotations

import os
import sys
import xml.etree.ElementTree as ET

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
LANGS_DIR = os.path.join(REPO_ROOT, "resources", "langs")
SOURCE_LANG = "en"


def count_unfinished(ts_path: str) -> int:
    """Count ``<translation type="unfinished">`` entries in a .ts file."""
    tree = ET.parse(ts_path)
    return sum(1 for t in tree.iter("translation") if t.get("type") == "unfinished")


def main() -> int:
    if not os.path.isdir(LANGS_DIR):
        print(f"ERROR: {LANGS_DIR} not found", file=sys.stderr)
        return 2

    offenders: list[tuple[str, int]] = []
    for lang in sorted(os.listdir(LANGS_DIR)):
        if lang == SOURCE_LANG:
            continue
        ts_path = os.path.join(LANGS_DIR, lang, f"komai_{lang}.ts")
        if not os.path.isfile(ts_path):
            continue
        n = count_unfinished(ts_path)
        if n > 0:
            offenders.append((lang, n))

    if not offenders:
        return 0

    print(
        "ERROR: cannot release with unfinished translation strings.",
        file=sys.stderr,
    )
    print("", file=sys.stderr)
    print("Languages with unfinished entries:", file=sys.stderr)
    for lang, n in offenders:
        print(f"  {lang}: {n} unfinished", file=sys.stderr)
    print("", file=sys.stderr)
    print(
        "Fill these in (see docs/maintainers/translations.md for the AI-assisted",
        file=sys.stderr,
    )
    print(
        "workflow) before bumping VERSION, or revert the VERSION change to keep",
        file=sys.stderr,
    )
    print("working without cutting a release.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
