#!/usr/bin/env sh

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo_root"

if ! command -v reuse >/dev/null 2>&1; then
    echo "reuse is not installed; skipping license check."
    exit 0
fi

license_file="$repo_root/LICENSES/GPL-3.0-or-later.txt"
if [ ! -f "$license_file" ]; then
    echo "Missing license text file: LICENSES/GPL-3.0-or-later.txt"
    echo "Create it first (for example: cp COPYING LICENSES/GPL-3.0-or-later.txt)."
    exit 1
fi

mit_license_file="$repo_root/LICENSES/MIT.txt"
if [ ! -f "$mit_license_file" ]; then
    echo "Missing license text file: LICENSES/MIT.txt"
    echo "Required for third-party icon assets under resources/icons/."
    exit 1
fi

status=0
if ! find src resources/qml -type f \( -iname "*.cpp" -o -iname "*.h" -o -iname "*.qml" \) -print0 | \
    xargs -0 -r reuse --no-multiprocessing lint-file; then
    status=1
fi

if ! find resources/icons -type f \( -iname "*.svg" -o -name "REUSE.toml" \) -print0 | \
    xargs -0 -r reuse --no-multiprocessing lint-file; then
    status=1
fi

if [ "$status" -ne 0 ]; then
    echo
    echo "REUSE compliance check failed."
    echo "If files are missing SPDX headers, run: just license-inject"
fi

exit "$status"
