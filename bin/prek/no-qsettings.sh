#!/usr/bin/env sh

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
target_dir="$repo_root/src"

if command -v rg >/dev/null 2>&1; then
    matches="$(rg -n --no-heading --color=never '\bQSettings\b' "$target_dir" || true)"
else
    matches="$(grep -R -n --line-number --color=never 'QSettings' "$target_dir" 2>/dev/null || true)"
fi

if [ -n "$matches" ]; then
    echo "QSettings usage is not allowed in Komai source files."
    echo "Use the YAML-backed UserSettings persistence under src/UserSettingsPage.* instead."
    echo
    echo "$matches"
    exit 1
fi

exit 0
