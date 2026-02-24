#!/usr/bin/env sh

# Adds SPDX headers to source files that do not have them yet.
# Exit codes:
# - 1: annotation changed files
# - 0: no changes needed

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo_root"

if ! command -v reuse >/dev/null 2>&1; then
    echo "reuse is not installed; cannot inject SPDX headers." >&2
    exit 1
fi

FILES=$(find src resources/qml -type f \( -iname "*.cpp" -o -iname "*.h" -o -iname "*.qml" \))

reuse annotate \
    --exclude-year \
    --skip-existing \
    --style=cppsingle \
    --copyright="Komai Contributors" \
    --license="GPL-3.0-or-later" \
    $FILES

git diff --exit-code
