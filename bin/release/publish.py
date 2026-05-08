#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Publish the built artefacts to a GitHub Release.

Extracts the relevant ``CHANGELOG.md`` section as release notes, then
calls ``gh release create`` with the three artefacts attached.

Refuses to run if any artefact is missing (run ``just release-build``
first) or if a GitHub release already exists for the tag. To republish,
the existing release must be deleted manually first
(``gh release delete v<VERSION>``).
"""

from __future__ import annotations

import argparse
import shlex
import sys
import tempfile
from pathlib import Path

from _lib import (
    REPO_ROOT,
    check_tools,
    expected_artefacts,
    extract_changelog_section,
    fail,
    info,
    read_version,
    run,
    tag_for,
)


def gh_release_exists(tag: str) -> bool:
    res = run(
        ["gh", "release", "view", tag, "--json", "tagName"],
        capture_output=True,
        text=True,
    )
    return res.returncode == 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the gh command and notes file path; do not publish.",
    )
    args = parser.parse_args()

    check_tools("gh")
    version = read_version()
    tag = tag_for(version)

    artefacts = expected_artefacts(version)
    missing = [p for p in artefacts if not p.is_file()]
    if missing:
        fail(
            "missing artefact(s) — run `just release-build` first:\n"
            + "\n".join(f"  - {p.relative_to(REPO_ROOT)}" for p in missing)
        )

    if gh_release_exists(tag):
        fail(
            f"GitHub release {tag} already exists. "
            f"Delete it first if you intend to republish: gh release delete {tag}"
        )

    notes = extract_changelog_section(version)
    if not notes.strip():
        fail(f"CHANGELOG.md has no content under '## {version}'.")

    notes_file = Path(
        tempfile.NamedTemporaryFile(
            mode="w",
            suffix=".md",
            prefix=f"komai-{tag}-notes-",
            delete=False,
            encoding="utf-8",
        ).name
    )
    notes_file.write_text(notes + "\n", encoding="utf-8")

    cmd = [
        "gh", "release", "create", tag,
        "--title", tag,
        "--notes-file", str(notes_file),
        *[str(p) for p in artefacts],
    ]

    try:
        if args.dry_run:
            info("[dry-run] would run:")
            info("  " + " ".join(shlex.quote(c) for c in cmd))
            info(f"[dry-run] notes file: {notes_file}")
            info("[dry-run] notes preview:")
            for line in notes.splitlines():
                info(f"    {line}")
            return 0

        info(f"Publishing {tag}...")
        res = run(cmd)
        if res.returncode != 0:
            fail(f"`gh release create` failed (exit {res.returncode})")
    finally:
        if not args.dry_run:
            notes_file.unlink(missing_ok=True)

    return 0


if __name__ == "__main__":
    sys.exit(main())
