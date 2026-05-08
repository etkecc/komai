#!/usr/bin/env python3
# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

"""End-to-end release orchestrator: validate, build, publish.

Local fallback for the parallel pipeline in ``.github/workflows/publish.yml``.
Runs the three phases sequentially in order, bailing on the first non-zero exit.
Pass ``--dry-run`` to do everything except the final ``gh release create``; the
publish phase will instead print the command and a notes preview.
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from _lib import fail, info, run

HERE = Path(__file__).resolve().parent


def run_phase(name: str, cmd: list[str]) -> None:
    info(f"=== Phase: {name} ===")
    res = run(cmd)
    if res.returncode != 0:
        fail(f"phase '{name}' failed (exit {res.returncode}); aborting.")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Skip the actual `gh release create` (publish runs in dry-run mode).",
    )
    args = parser.parse_args()

    run_phase("validate", [sys.executable, str(HERE / "validate.py")])
    run_phase("build", [sys.executable, str(HERE / "build.py")])
    publish_cmd = [sys.executable, str(HERE / "publish.py")]
    if args.dry_run:
        publish_cmd.append("--dry-run")
    run_phase("publish", publish_cmd)

    info("=== Done ===")
    return 0


if __name__ == "__main__":
    sys.exit(main())
