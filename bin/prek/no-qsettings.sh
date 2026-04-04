#!/usr/bin/env sh

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
target_dir="$repo_root/src"

matches="$(grep -R -n -w --line-number --color=never --exclude-dir=target 'QSettings' "$target_dir" 2>/dev/null || true)"

if [ -n "$matches" ]; then
    echo "QSettings usage is not allowed in Komai source files."
    echo "Use the YAML-backed settings persistence under src/settings/ instead."
    echo
    echo "$matches"
    exit 1
fi

exit 0
