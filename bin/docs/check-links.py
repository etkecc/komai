#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path
from urllib.parse import unquote


LINK_RE = re.compile(r"\[[^\]]+\]\(([^)]+)\)")
SCHEME_RE = re.compile(r"^[a-zA-Z][a-zA-Z0-9+.\-]*:")
EXCLUDED_DIRS = {".git", "var"}


def parse_target(raw: str) -> str:
    target = raw.strip()
    if not target:
        return ""

    # Markdown allows optional angle brackets around the target.
    if target.startswith("<") and target.endswith(">"):
        target = target[1:-1].strip()
    else:
        # Handle optional title: (path "title")
        match = re.match(r"^(\S+)\s+['\"].*['\"]$", target)
        if match:
            target = match.group(1)

    return target


def is_external_target(target: str) -> bool:
    if target.startswith(("#", "mailto:", "http://", "https://", "data:")):
        return True
    return bool(SCHEME_RE.match(target))


def iter_markdown_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for path in root.rglob("*.md"):
        rel = path.relative_to(root)
        if any(part in EXCLUDED_DIRS for part in rel.parts):
            continue
        if path.is_file():
            files.append(path)
    return sorted(files)


def resolve_target(markdown_file: Path, target: str, root: Path) -> Path:
    local_target = target.split("#", 1)[0].split("?", 1)[0]
    local_target = unquote(local_target)
    if local_target.startswith("/"):
        return (root / local_target.lstrip("/")).resolve()
    return (markdown_file.parent / local_target).resolve()


def check_file(path: Path, root: Path) -> list[str]:
    errors: list[str] = []
    lines = path.read_text(encoding="utf-8").splitlines()

    for line_no, line in enumerate(lines, start=1):
        for match in LINK_RE.finditer(line):
            raw_target = match.group(1)
            target = parse_target(raw_target)
            if not target or is_external_target(target):
                continue

            local_target = target.split("#", 1)[0].split("?", 1)[0]
            if not local_target:
                continue

            resolved = resolve_target(path, target, root)
            if resolved.exists():
                continue

            rel_file = path.relative_to(root)
            rel_expected = (
                resolved.relative_to(root)
                if resolved.is_relative_to(root)
                else resolved
            )
            errors.append(
                f"{rel_file}:{line_no}: broken link target '{target}' "
                f"(expected '{rel_expected}')"
            )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check repository Markdown relative links for missing targets."
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[2],
        help="Repository root to scan (defaults to script's repo root).",
    )
    args = parser.parse_args()

    root = args.root.resolve()
    files = iter_markdown_files(root)

    all_errors: list[str] = []
    for path in files:
        all_errors.extend(check_file(path, root))

    if all_errors:
        print("Broken Markdown links found:", file=sys.stderr)
        for err in all_errors:
            print(f"  - {err}", file=sys.stderr)
        return 1

    print(f"Markdown links check passed ({len(files)} files scanned).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
