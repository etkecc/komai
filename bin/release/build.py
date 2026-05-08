#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""Build all three release artefacts (AppImage, Flatpak, Snap).

Drives the existing ``just appimage-build-docker`` / ``just flatpak-build``
/ ``just snap-build-docker`` recipes sequentially, then verifies each
expected output file lives at the path ``publish.py`` will look for.
"""

from __future__ import annotations

import argparse
import sys
import time

from _lib import (
    REPO_ROOT,
    check_tools,
    expected_artefacts,
    fail,
    info,
    read_version,
    run,
)


def run_just(target: str) -> None:
    info(f">>> just {target}")
    started = time.monotonic()
    res = run(["just", target])
    elapsed = time.monotonic() - started
    if res.returncode != 0:
        fail(f"`just {target}` failed (exit {res.returncode}) after {elapsed:.0f}s")
    info(f"    finished in {elapsed:.0f}s")


def main() -> int:
    argparse.ArgumentParser(description=__doc__).parse_args()

    check_tools("just", "docker", "flatpak-builder")
    version = read_version()
    info(f"Building release artefacts for v{version}...")

    run_just("appimage-build-docker")
    run_just("flatpak-build")
    run_just("snap-build-docker")

    info("Verifying expected artefact paths...")
    paths = expected_artefacts(version)
    missing = [p for p in paths if not p.is_file()]
    if missing:
        fail(
            "build completed but expected artefacts are missing:\n"
            + "\n".join(f"  - {p.relative_to(REPO_ROOT)}" for p in missing)
        )

    info("All artefacts produced:")
    for p in paths:
        size_mb = p.stat().st_size / 1024 / 1024
        info(f"  - {p.relative_to(REPO_ROOT)} ({size_mb:.2f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
