#!/usr/bin/env sh
#
# Qt's default Button from QtQuick.Controls is not theme-color-aware.
# KomaiButton extends Button with Komai palette integration: proper
# highlight/hover/pressed colors, dynamic icon tinting, and consistent
# sizing across the app.
#
# Using bare Button {} anywhere except KomaiButton.qml itself (where it
# serves as the base type) will produce buttons that ignore the active
# theme, so we reject it at commit time.

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"

if [ ! -f "$repo_root/CMakeLists.txt" ]; then
    echo "error: could not locate repository root (expected CMakeLists.txt at $repo_root)" >&2
    exit 1
fi

target_dir="$repo_root/resources/qml"

# KomaiButton.qml itself extends Button — that's the one allowed usage.
exclude="--exclude=KomaiButton.qml"

matches="$(grep -R -n --color=never $exclude -P '^\s+Button\s*\{' "$target_dir" 2>/dev/null || true)"

if [ -n "$matches" ]; then
    echo "Bare 'Button {' usage found in QML files."
    echo "Use KomaiButton (or Components.KomaiButton) instead for theme-aware styling."
    echo
    echo "$matches"
    exit 1
fi

exit 0
