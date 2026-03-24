#!/usr/bin/env sh
#
# Qt's default controls from QtQuick.Controls are not theme-color-aware.
# Komai provides themed wrappers (KomaiButton, KomaiComboBox, etc.) that
# integrate with the active palette for consistent colors, icon tinting,
# and sizing across the app.
#
# Using bare controls anywhere except the Komai wrapper definitions
# themselves (where the base type is the root element) will produce
# controls that ignore the active theme, so we reject it at commit time.
#
# Intentional exclusions beyond the wrapper files:
#   - composer/MessageInput.qml — heavily customized input with its own
#     completion system and sizing constraints

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)"

if [ ! -f "$repo_root/CMakeLists.txt" ]; then
    echo "error: could not locate repository root (expected CMakeLists.txt at $repo_root)" >&2
    exit 1
fi

target_dir="$repo_root/resources/qml"

# Each Komai wrapper extends the bare control — exclude the wrapper files.
# MessageInput is the composer with special TextArea handling.
excludes="
--exclude=KomaiButton.qml
--exclude=KomaiComboBox.qml
--exclude=KomaiSpinBox.qml
--exclude=KomaiTextField.qml
--exclude=KomaiTextArea.qml
--exclude=KomaiTabButton.qml
--exclude=KomaiToolTip.qml
--exclude=MessageInput.qml
"

# Match bare control types as QML object declarations (type followed by {).
control_pattern='^\s+(Button|ComboBox|SpinBox|TextField|TextArea|TabButton)\s*\{'
control_matches="$(grep -R -n --color=never $excludes -P "$control_pattern" "$target_dir" 2>/dev/null || true)"

if [ -n "$control_matches" ]; then
    echo "Bare Qt control usage found in QML files."
    echo "Use the Komai equivalent instead for theme-aware styling:"
    echo "  Button    → KomaiButton"
    echo "  ComboBox  → KomaiComboBox"
    echo "  SpinBox   → KomaiSpinBox"
    echo "  TextField → KomaiTextField"
    echo "  TextArea  → KomaiTextArea"
    echo "  TabButton → KomaiTabButton"
    echo
    echo "$control_matches"
    exit 1
fi

tooltip_pattern='^\s+ToolTip(\.|\s*\{)'
tooltip_matches="$(grep -R -n --color=never $excludes -P "$tooltip_pattern" "$target_dir" 2>/dev/null || true)"

if [ -n "$tooltip_matches" ]; then
    echo "Bare Qt ToolTip usage found in QML files."
    echo "Use KomaiToolTip or a Komai wrapper that already exposes themed tooltips."
    echo
    echo "$tooltip_matches"
    exit 1
fi

exit 0
