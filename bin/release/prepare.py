#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Prepare a new Komai release.

Bumps VERSION.txt (CalVer YYYY.MM.DD.N) and propagates the new value to
every drift surface the ``version-drift`` pre-commit hook validates:

    * ``VERSION.txt``
    * ``etc/packaging/archlinux/komai/PKGBUILD``      (pkgver, pkgrel)
    * ``etc/packaging/archlinux/komai-bin/PKGBUILD``  (pkgver, pkgrel)
    * ``resources/komai.appdata.xml.in``    (<release> entry)
    * ``CHANGELOG.md``                      (new section)

Without an argument, the next version is computed from the current UTC date:

    * if the current VERSION.txt's date prefix matches today, the trailing
      counter is incremented;
    * otherwise, the new version is ``<today>.0``.

Pass an explicit CalVer argument (for example ``2026.04.22.3``) to override
the computed value.
"""

from __future__ import annotations

import argparse
import datetime as dt
import re
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[2]

VERSION_FILE = REPO_ROOT / "VERSION.txt"
PKGBUILD_FILE = REPO_ROOT / "etc/packaging/archlinux/komai/PKGBUILD"
PKGBUILD_BIN_FILE = REPO_ROOT / "etc/packaging/archlinux/komai-bin/PKGBUILD"
APPDATA_FILE = REPO_ROOT / "resources/komai.appdata.xml.in"
CHANGELOG_FILE = REPO_ROOT / "CHANGELOG.md"

CALVER_RE = re.compile(r"^(\d{4})\.(\d{2})\.(\d{2})\.(\d+)$")


def parse_calver(text: str) -> tuple[int, int, int, int]:
    m = CALVER_RE.match(text.strip())
    if not m:
        raise ValueError(
            f"Not a valid CalVer version: {text!r} (expected YYYY.MM.DD.N)"
        )
    return (int(m.group(1)), int(m.group(2)), int(m.group(3)), int(m.group(4)))


def compute_next_version(current: str, today_utc: dt.date) -> str:
    today_prefix = today_utc.strftime("%Y.%m.%d")
    year, month, day, counter = parse_calver(current)
    cur_prefix = f"{year:04d}.{month:02d}.{day:02d}"
    if cur_prefix == today_prefix:
        return f"{today_prefix}.{counter + 1}"
    return f"{today_prefix}.0"


def edit_version(new: str) -> None:
    VERSION_FILE.write_text(new + "\n", encoding="utf-8")


def edit_pkgbuild(path: Path, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    text, n_ver = re.subn(
        r"^pkgver=.*$", f"pkgver={new}", text, count=1, flags=re.MULTILINE
    )
    text, n_rel = re.subn(
        r"^pkgrel=.*$", "pkgrel=1", text, count=1, flags=re.MULTILINE
    )
    if n_ver != 1 or n_rel != 1:
        raise RuntimeError(
            f"{path.name}: failed to update pkgver/pkgrel "
            f"(pkgver matches={n_ver}, pkgrel matches={n_rel})"
        )
    path.write_text(text, encoding="utf-8")


def edit_appdata(new: str, release_date: dt.date) -> None:
    text = APPDATA_FILE.read_text(encoding="utf-8")
    entry = f'    <release version="{new}" date="{release_date.isoformat()}"/>'

    empty_re = re.compile(r"(<releases>)\s*(</releases>)")
    if empty_re.search(text):
        text = empty_re.sub(rf"\1\n{entry}\n  \2", text, count=1)
    else:
        text, n = re.subn(r"(<releases>\n)", rf"\1{entry}\n", text, count=1)
        if n != 1:
            raise RuntimeError(
                "appdata.xml.in: could not find <releases> block to update"
            )
    APPDATA_FILE.write_text(text, encoding="utf-8")


def edit_changelog(new: str) -> None:
    text = CHANGELOG_FILE.read_text(encoding="utf-8")
    section = (
        f"## {new}\n"
        f"\n"
        f"<!-- TODO: fill in release notes -->\n"
    )

    placeholder_re = re.compile(r"No changelog entries yet\.\n")
    if placeholder_re.search(text):
        text = placeholder_re.sub(section, text, count=1)
    else:
        text, n = re.subn(
            r"(^# Changelog\n\n)",
            rf"\1{section}\n",
            text,
            count=1,
            flags=re.MULTILINE,
        )
        if n != 1:
            raise RuntimeError(
                "CHANGELOG.md: could not find '# Changelog' heading to insert after"
            )
    CHANGELOG_FILE.write_text(text, encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Prepare a new Komai release by bumping VERSION.txt and propagating the change.",
    )
    parser.add_argument(
        "version",
        nargs="?",
        help="Explicit CalVer version (e.g. 2026.04.22.3). "
        "Omit to auto-compute from today's UTC date.",
    )
    args = parser.parse_args()

    today_utc = dt.datetime.now(dt.timezone.utc).date()
    current = VERSION_FILE.read_text(encoding="utf-8").strip()

    try:
        cur_parts = parse_calver(current)
    except ValueError as exc:
        print(f"ERROR: current VERSION.txt is invalid: {exc}", file=sys.stderr)
        return 1

    new = args.version if args.version else compute_next_version(current, today_utc)

    try:
        new_parts = parse_calver(new)
    except ValueError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1

    if new_parts <= cur_parts:
        print(
            f"ERROR: new version {new} is not greater than current {current}.",
            file=sys.stderr,
        )
        return 1

    edit_version(new)
    edit_pkgbuild(PKGBUILD_FILE, new)
    edit_pkgbuild(PKGBUILD_BIN_FILE, new)
    edit_appdata(new, today_utc)
    edit_changelog(new)

    tag = f"v{new}"
    print(f"Prepared release {tag} (from v{current}).")
    print()
    print("Next steps:")
    print("  1. Edit CHANGELOG.md: replace the TODO with the actual release notes.")
    print("  2. Review changes:   git diff")
    print(f"  3. Commit:           git commit -am 'Release {tag}'")
    print(f"  4. Tag:              git tag {tag}")
    print("  5. Push:             git push && git push --tags")
    return 0


if __name__ == "__main__":
    sys.exit(main())
