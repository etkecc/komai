#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Keep the Flatpak manifest's bundled Rust toolchain in sync with rust-toolchain.toml.

The Flatpak build can't use rustup, so cc.etke.komai.yaml bundles the standalone
Rust installer tarballs for each architecture. rust-toolchain.toml is the source
of truth for the version (and what Renovate bumps); this script verifies that
every bundled tarball matches it (`check`), and rewrites the tarball URLs and
sha256s from the official channel manifest for that version (`update-lock`).

Stdlib only.
"""

from __future__ import annotations

import argparse
import pathlib
import re
import sys
import tomllib
import urllib.request

TOOLCHAIN_FILE = pathlib.Path("rust-toolchain.toml")
MANIFEST_FILE = pathlib.Path("etc/packaging/flatpak/cc.etke.komai.yaml")

# Architectures the manifest must bundle a toolchain for.
ARCHES = ("x86_64", "aarch64")

CHANNEL_MANIFEST_URL = "https://static.rust-lang.org/dist/channel-rust-{version}.toml"

_TARBALL_URL_RE = re.compile(
    r"url: https://static\.rust-lang\.org/dist/[^/\s]+/"
    r"rust-(?P<version>[0-9]+\.[0-9]+\.[0-9]+)-(?P<arch>[a-z0-9_]+)-unknown-linux-gnu\.tar\.xz"
)

# A tarball's url line plus the sha256 line that follows it.
_TARBALL_ENTRY_RE = re.compile(
    r"(?P<indent>[ \t]*)url: https://static\.rust-lang\.org/dist/[^/\s]+/"
    r"rust-[0-9.]+-(?P<arch>[a-z0-9_]+)-unknown-linux-gnu\.tar\.xz\n"
    r"[ \t]*sha256: [0-9a-f]{64}"
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

    if fail:
        print("\nRun 'just flatpak-rust-update-lock' to re-pin the bundled tarballs.", file=sys.stderr)
    return 1 if fail else 0


def cmd_update_lock(repo_root: pathlib.Path) -> int:
    version = _toolchain_version(repo_root)
    url = CHANNEL_MANIFEST_URL.format(version=version)
    print(f"Fetching {url}", file=sys.stderr)
    with urllib.request.urlopen(url, timeout=60) as resp:
        channel = tomllib.loads(resp.read().decode("utf-8"))
    targets = channel["pkg"]["rust"]["target"]

    def replace(m: re.Match[str]) -> str:
        target = targets[f"{m['arch']}-unknown-linux-gnu"]
        indent = m["indent"]
        return f"{indent}url: {target['xz_url']}\n{indent}sha256: {target['xz_hash']}"

    manifest_path = repo_root / MANIFEST_FILE
    manifest, count = _TARBALL_ENTRY_RE.subn(replace, manifest_path.read_text(encoding="utf-8"))
    if count != len(ARCHES):
        print(f"ERROR: expected {len(ARCHES)} Rust tarball entries in {MANIFEST_FILE}, found {count}", file=sys.stderr)
        return 1
    manifest_path.write_text(manifest, encoding="utf-8")
    print(f"Pinned {MANIFEST_FILE} to Rust {version}", file=sys.stderr)
    return cmd_check(repo_root)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("command", choices=["check", "update-lock"])
    parser.add_argument("--repo-root", type=pathlib.Path, default=pathlib.Path("."))
    args = parser.parse_args()

    if args.command == "update-lock":
        return cmd_update_lock(args.repo_root)
    return cmd_check(args.repo_root)


if __name__ == "__main__":
    sys.exit(main())
