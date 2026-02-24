#!/usr/bin/env sh

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo_root"

if ! command -v reuse >/dev/null 2>&1; then
    echo "reuse is not installed; skipping license check."
    exit 0
fi

set +e
reuse --no-multiprocessing lint
status=$?
set -e

if [ "$status" -ne 0 ]; then
    echo
    echo "REUSE compliance check failed."
    echo "If files are missing SPDX headers, run: just license-inject"
fi

exit "$status"
