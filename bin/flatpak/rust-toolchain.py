#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Keep the Flatpak manifest's bundled Rust toolchain in sync with rust-toolchain.toml.

The Flatpak build can't use rustup, so cc.etke.komai.yaml bundles the standalone
Rust installer tarballs for each architecture. rust-toolchain.toml is the source
of truth for the version (and what Renovate bumps); this script verifies that
every bundled tarball matches it.

Stdlib only.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
import tomllib

TOOLCHAIN_FILE = pathlib.Path("rust-toolchain.toml")
MANIFEST_FILE = pathlib.Path("etc/packaging/flatpak/cc.etke.komai.yaml")

# Architectures the manifest must bundle a toolchain for.
ARCHES = ("x86_64", "aarch64")

_TARBALL_URL_RE = re.compile(
    r"url: https://static\.rust-lang\.org/dist/[^/\s]+/"
    r"rust-(?P<version>[0-9]+\.[0-9]+\.[0-9]+)-(?P<arch>[a-z0-9_]+)-unknown-linux-gnu\.tar\.xz"
)


def _toolchain_version(repo_root: pathlib.Path) -> str:
    with (repo_root / TOOLCHAIN_FILE).open("rb") as f:
        channel = tomllib.load(f).get("toolchain", {}).get("channel", "")
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", channel):
        raise SystemExit(f"ERROR: {TOOLCHAIN_FILE} channel is not an exact X.Y.Z version: '{channel}'")
    return channel


def cmd_check(repo_root: pathlib.Path) -> int:
    expected = _toolchain_version(repo_root)
    manifest = (repo_root / MANIFEST_FILE).read_text(encoding="utf-8")

    found: dict[str, list[str]] = {}
    for m in _TARBALL_URL_RE.finditer(manifest):
        found.setdefault(m["arch"], []).append(m["version"])

    fail = False
    for arch in ARCHES:
        versions = found.get(arch, [])
        if len(versions) != 1:
            print(f"ERROR: {MANIFEST_FILE} has {len(versions)} Rust tarballs for {arch}, expected 1", file=sys.stderr)
            fail = True
        elif versions[0] != expected:
            print(
                f"ERROR: {MANIFEST_FILE} bundles Rust {versions[0]} for {arch}, "
                f"but {TOOLCHAIN_FILE} pins {expected}",
                file=sys.stderr,
            )
            fail = True

    return 1 if fail else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=["check"])
    parser.add_argument("--repo-root", type=pathlib.Path, default=pathlib.Path("."))
    args = parser.parse_args()

    return cmd_check(args.repo_root)


if __name__ == "__main__":
    sys.exit(main())
