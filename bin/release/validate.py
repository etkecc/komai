#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Validate release prerequisites for the version in ``VERSION.txt``.

Runs a series of independent gates and exits non-zero on the first
failure with a clear message. Intended to run before ``build.py`` /
``publish.py`` so failures surface before any expensive build work.

Gates (in order):

    1. Git working tree is clean.
    2. ``HEAD`` carries the tag ``v<VERSION>``.
    3. The remote ``origin`` already has that tag pointing at ``HEAD``.
    4. ``CHANGELOG.md`` has a non-empty ``## <VERSION>`` section.
    5. No version drift across PKGBUILD / appdata / CHANGELOG
       (delegated to ``bin/prek/version-drift.sh``).
    6. ``gh`` CLI is authenticated, with ``repo`` scope.
    7. ``gh`` resolves the GitHub repo for the current remote.
    8. No GitHub release already exists for ``v<VERSION>``.
"""

from __future__ import annotations

import argparse
import json
import sys

from _lib import (
    REPO_ROOT,
    check_tools,
    extract_changelog_section,
    fail,
    info,
    read_version,
    run,
    tag_for,
)


def gate_clean_tree() -> None:
    info("[1/8] Working tree clean")
    res = run(
        ["git", "status", "--porcelain"], capture_output=True, text=True, check=True
    )
    if res.stdout.strip():
        fail("working tree has uncommitted changes:\n" + res.stdout)


def gate_head_tagged(tag: str) -> None:
    info(f"[2/8] HEAD carries tag {tag}")
    res = run(
        ["git", "tag", "--points-at", "HEAD"],
        capture_output=True,
        text=True,
        check=True,
    )
    tags = res.stdout.split()
    if tag not in tags:
        fail(
            f"HEAD is not tagged {tag} (tags at HEAD: {tags or 'none'}). "
            f"Tag the release commit first: git tag {tag}"
        )


def gate_tag_on_origin(tag: str) -> None:
    info(f"[3/8] Tag {tag} on origin and matches HEAD")
    res = run(
        ["git", "ls-remote", "--tags", "origin", tag],
        capture_output=True,
        text=True,
        check=True,
    )
    if not res.stdout.strip():
        fail(f"tag {tag} not found on origin. Push it: git push origin {tag}")
    remote_sha = res.stdout.split()[0]
    head = run(
        ["git", "rev-parse", "HEAD"], capture_output=True, text=True, check=True
    )
    head_sha = head.stdout.strip()
    if remote_sha != head_sha:
        fail(
            f"tag {tag} on origin points to {remote_sha[:10]}, "
            f"but HEAD is {head_sha[:10]}."
        )


def gate_changelog(version: str) -> None:
    info(f"[4/8] CHANGELOG section for {version} non-empty")
    body = extract_changelog_section(version)
    if not body.strip():
        fail(
            f"CHANGELOG.md has no non-empty section under '## {version}'. "
            f"Run `just release-prepare {version}` and fill in the entry."
        )


def gate_drift_check() -> None:
    info("[5/8] No version drift across release-bearing files")
    res = run(
        [str(REPO_ROOT / "bin/prek/version-drift.sh")],
        capture_output=True,
        text=True,
    )
    if res.returncode != 0:
        fail(
            "version-drift hook reports inconsistencies:\n"
            + (res.stderr.strip() or res.stdout.strip())
        )


def gate_gh_authed() -> None:
    info("[6/8] gh CLI authenticated with repo scope")
    res = run(["gh", "auth", "status"], capture_output=True, text=True)
    if res.returncode != 0:
        fail("gh CLI is not authenticated. Run: gh auth login")
    combined = res.stdout + res.stderr
    if "'repo'" not in combined and "repo," not in combined and " repo\n" not in combined:
        fail("gh token is missing 'repo' scope. Re-auth: gh auth refresh -s repo")


def gate_repo_context() -> str:
    info("[7/8] gh resolves a GitHub repo for the current remote")
    res = run(
        ["gh", "repo", "view", "--json", "nameWithOwner"],
        capture_output=True,
        text=True,
    )
    if res.returncode != 0:
        fail("gh cannot resolve repo context:\n" + (res.stderr or res.stdout))
    repo = json.loads(res.stdout)["nameWithOwner"]
    info(f"        repo: {repo}")
    return repo


def gate_no_existing_release(tag: str) -> None:
    info(f"[8/8] No existing GitHub release for {tag}")
    res = run(
        ["gh", "release", "view", tag, "--json", "tagName"],
        capture_output=True,
        text=True,
    )
    if res.returncode == 0:
        fail(
            f"GitHub release {tag} already exists. "
            f"Delete it first if you intend to republish: gh release delete {tag}"
        )


def main() -> int:
    argparse.ArgumentParser(description=__doc__).parse_args()

    check_tools("git", "gh")
    version = read_version()
    tag = tag_for(version)
    info(f"Validating release prerequisites for {tag}...")

    gate_clean_tree()
    gate_head_tagged(tag)
    gate_tag_on_origin(tag)
    gate_changelog(version)
    gate_drift_check()
    gate_gh_authed()
    gate_repo_context()
    gate_no_existing_release(tag)

    info(f"OK: ready to publish {tag}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
