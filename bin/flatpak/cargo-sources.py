#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Generate cargo-sources.json for Flatpak offline Rust builds.

Reads src/rust/Cargo.lock and produces a JSON array of Flatpak source
definitions that vendor each crate for offline builds.

Uses only Python stdlib (tomllib requires Python 3.11+).

Each crates.io dependency becomes two Flatpak source entries:
  1. An archive source that extracts the .crate tarball into cargo/vendor/<name>-<version>/
  2. An inline source that writes .cargo-checksum.json (required by cargo's vendor protocol)

A final inline source writes .cargo/config.toml to redirect crates-io to the vendor directory.
"""

import json
import sys
import tomllib
from pathlib import Path

CRATES_IO = "https://static.crates.io/crates"
CARGO_HOME = "cargo"
VENDOR_DIR = f"{CARGO_HOME}/vendor"


def generate_sources(lockfile: Path) -> list[dict]:
    """Parse Cargo.lock and generate Flatpak source entries."""
    with open(lockfile, "rb") as f:
        lock = tomllib.load(f)

    sources = []
    for pkg in lock.get("package", []):
        name = pkg["name"]
        version = pkg["version"]
        source = pkg.get("source", "")

        # Only handle crates.io packages (registry+...)
        if not source.startswith("registry+"):
            continue

        checksum = pkg.get("checksum")
        if not checksum:
            print(
                f"  Warning: no checksum for {name} {version}, skipping",
                file=sys.stderr,
            )
            continue

        crate_url = f"{CRATES_IO}/{name}/{name}-{version}.crate"
        dest = f"{VENDOR_DIR}/{name}-{version}"

        # Archive source: download and extract the .crate tarball
        sources.append(
            {
                "type": "archive",
                "archive-type": "tar-gzip",
                "url": crate_url,
                "sha256": checksum,
                "dest": dest,
            }
        )

        # Checksum file: cargo's vendor protocol requires this
        checksum_json = json.dumps({"files": {}, "package": checksum})
        sources.append(
            {
                "type": "inline",
                "contents": checksum_json,
                "dest": dest,
                "dest-filename": ".cargo-checksum.json",
            }
        )

    return sources


def generate_cargo_config() -> dict:
    """Generate the .cargo/config.toml source for vendored builds."""
    config_content = (
        '[source.crates-io]\n'
        'replace-with = "vendored-sources"\n'
        "\n"
        "[source.vendored-sources]\n"
        f'directory = "{VENDOR_DIR}"\n'
    )
    return {
        "type": "inline",
        "contents": config_content,
        "dest": CARGO_HOME,
        "dest-filename": "config.toml",
    }


def main():
    if len(sys.argv) < 2:
        repo_root = Path(__file__).resolve().parent.parent.parent
    else:
        repo_root = Path(sys.argv[1])

    lockfile = repo_root / "src" / "rust" / "Cargo.lock"
    output = repo_root / "var" / "build" / "flatpak" / "cargo-sources.json"
    output.parent.mkdir(parents=True, exist_ok=True)

    if not lockfile.exists():
        print(f"Error: {lockfile} not found", file=sys.stderr)
        sys.exit(1)

    print(f"Reading {lockfile}", file=sys.stderr)
    sources = generate_sources(lockfile)
    sources.append(generate_cargo_config())

    with open(output, "w") as f:
        json.dump(sources, f, indent=2)
        f.write("\n")

    crate_count = sum(1 for s in sources if s["type"] == "archive")
    print(f"Wrote {crate_count} crates ({len(sources)} entries) to {output}", file=sys.stderr)


if __name__ == "__main__":
    main()
