#!/usr/bin/env sh

# Runs license header annotation over source files.
# Exit codes:
# - 1: annotation changed files
# - 0: no changes needed

set -eu

FILES=$(find src resources/qml -type f \( -iname "*.cpp" -o -iname "*.h" -o -iname "*.qml" \))

reuse annotate --exclude-year --style=cppsingle --copyright="Komai Contributors" --license="GPL-3.0-or-later" $FILES

git diff --exit-code
