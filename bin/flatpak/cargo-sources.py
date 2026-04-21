#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Vendor Cargo dependencies for the offline Flatpak build.

Runs `cargo vendor` locally to materialize every dependency (crates.io AND git)
into a flat `vendor/` directory, and emits a Flatpak sources JSON file that:
  1. Copies the vendor directory into $CARGO_HOME/vendor inside the sandbox.
  2. Writes a `.cargo/config.toml` with the source redirects produced by
     `cargo vendor` so cargo resolves every registry and git source locally.

This replaces the previous per-crate archive approach, which could only handle
crates.io packages. The matrix-sdk bump (see
var/plans/matrix-sdk-bump-and-cleartext-send-bug.md) pulls matrix-sdk and ruma
from git SHAs, which cannot be fetched via flatpak's `archive` source.

Uses only Python stdlib.
"""

import json
import subprocess
import sys
from pathlib import Path

CARGO_HOME = "cargo"
VENDOR_DIR_REL = f"{CARGO_HOME}/vendor"


def run_cargo_vendor(manifest_path: Path, vendor_dir: Path) -> str:
    """Run `cargo vendor` and return the cargo-config snippet it prints."""
    vendor_dir.parent.mkdir(parents=True, exist_ok=True)
    cmd = [
        "cargo",
        "vendor",
        "--manifest-path",
        str(manifest_path),
        "--locked",
        str(vendor_dir),
    ]
    print(f"Running: {' '.join(cmd)}", file=sys.stderr)
    result = subprocess.run(cmd, check=True, capture_output=True, text=True)
    return result.stdout


def rewrite_config_for_sandbox(config_snippet: str) -> str:
    """Replace the host-absolute vendor directory with the in-sandbox path."""
    lines = []
    for line in config_snippet.splitlines():
        # `cargo vendor` emits e.g.
        #   directory = "/home/.../var/build/flatpak/cargo-vendor"
        # In the sandbox, everything lives under $CARGO_HOME (= cargo/).
        if line.strip().startswith("directory ="):
            lines.append(f'directory = "{VENDOR_DIR_REL}"')
        else:
            lines.append(line)
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv) < 2:
        repo_root = Path(__file__).resolve().parent.parent.parent
    else:
        repo_root = Path(sys.argv[1])

    manifest = repo_root / "src" / "rust" / "Cargo.toml"
    if not manifest.exists():
        print(f"Error: {manifest} not found", file=sys.stderr)
        sys.exit(1)

    vendor_dir = repo_root / "var" / "build" / "flatpak" / "cargo-vendor"
    output = repo_root / "var" / "build" / "flatpak" / "cargo-sources.json"
    output.parent.mkdir(parents=True, exist_ok=True)

    config_snippet = run_cargo_vendor(manifest, vendor_dir)
    config_contents = rewrite_config_for_sandbox(config_snippet)

    sources = [
        # Copy the locally-populated vendor tree into the sandbox. flatpak-builder
        # resolves `path` entries from a JSON-included source list relative to
        # the *yaml manifest* (etc/packaging/flatpak/cc.etke.komai.yaml), not
        # relative to this JSON file. So we walk up three levels to the repo
        # root and then down into var/build/flatpak/cargo-vendor.
        {
            "type": "dir",
            "path": "../../../var/build/flatpak/cargo-vendor",
            "dest": VENDOR_DIR_REL,
        },
        # Redirect crates.io + every git source onto the vendor tree.
        {
            "type": "inline",
            "contents": config_contents,
            "dest": CARGO_HOME,
            "dest-filename": "config.toml",
        },
    ]

    with open(output, "w") as f:
        json.dump(sources, f, indent=2)
        f.write("\n")

    print(
        f"Vendored to {vendor_dir}; wrote {len(sources)} flatpak source entries to {output}",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
