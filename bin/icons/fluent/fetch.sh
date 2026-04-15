#!/usr/bin/env sh
#
# Fetch a single Fluent icon by upstream relative path and vendor it under
# resources/icons/fluent/<REL_PATH>, then wire a qrc alias for runtime usage.
#
# Usage:
#   fetch.sh REL_PATH ALIAS_SVG_NAME
# Example:
#   fetch.sh assets/Search/SVG/ic_fluent_search_24_regular.svg search

set -eu

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH= cd -- "$script_dir/../../.." && pwd)"
cd "$repo_root"

lock_file="$script_dir/repo.conf"
version_ref_file="$script_dir/VERSION_REF"

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 REL_PATH ALIAS_SVG_NAME" >&2
    exit 1
fi

rel_path="$1"
alias_name_input="$2"

case "$rel_path" in
    *.svg)
        ;;
    *)
        echo "REL_PATH must point to an .svg file (got: $rel_path)" >&2
        exit 1
        ;;
esac

if [ ! -f "$lock_file" ]; then
    echo "Missing lock file: $lock_file" >&2
    exit 1
fi

if [ ! -f "$version_ref_file" ]; then
    echo "Missing version ref file: $version_ref_file" >&2
    exit 1
fi

raw_ref="$(awk 'NF > 0 && $1 !~ /^#/ { print $1; exit }' "$version_ref_file")"
if [ -z "$raw_ref" ]; then
    echo "Set a version/ref in bin/icons/fluent/VERSION_REF first." >&2
    exit 1
fi

# shellcheck source=/dev/null
. "$lock_file"

if [ -z "${FLUENT_REPO:-}" ]; then
    echo "Set FLUENT_REPO in bin/icons/fluent/repo.conf first." >&2
    exit 1
fi

alias_base="${alias_name_input##*/}"
case "$alias_base" in
    *.svg)
        has_svg_ext=1
        ;;
    *.*)
        echo "ALIAS_SVG_NAME must end with .svg (or omit extension): $alias_name_input" >&2
        exit 1
        ;;
    *)
        has_svg_ext=0
        ;;
esac

case "$alias_name_input" in
    */*)
        alias_rel="$alias_name_input"
        ;;
    *)
        alias_rel="ui/$alias_name_input"
        ;;
esac

if [ "$has_svg_ext" -eq 0 ]; then
    alias_rel="${alias_rel}.svg"
fi

case "$alias_rel" in
    icons/*)
        alias_rel="${alias_rel#icons/}"
        ;;
esac

dst="$repo_root/resources/icons/fluent/$rel_path"
url_rel_path="$(python3 - "$rel_path" <<'PY'
import sys
import urllib.parse

# Keep path separators and existing percent-encoding while escaping spaces and
# other URL-unsafe characters in Fluent upstream paths.
print(urllib.parse.quote(sys.argv[1], safe="/-._~%"))
PY
)"
url="https://raw.githubusercontent.com/$FLUENT_REPO/$raw_ref/$url_rel_path"

mkdir -p "$(dirname "$dst")"
curl -fsSL "$url" -o "$dst"

qrc_file="$repo_root/resources/res.qrc"
alias_path="icons/$alias_rel"
source_path="icons/fluent/$rel_path"
new_entry="        <file alias=\"$alias_path\">$source_path</file>"

tmp_qrc="$(mktemp)"
trap 'rm -f "$tmp_qrc"' EXIT INT TERM

if grep -Fq "<file alias=\"$alias_path\">" "$qrc_file"; then
    sed "s#^[[:space:]]*<file alias=\"$alias_path\">.*</file>#$new_entry#" "$qrc_file" > "$tmp_qrc"
    mv "$tmp_qrc" "$qrc_file"
elif grep -Fq "<file>$alias_path</file>" "$qrc_file"; then
    sed "s#^[[:space:]]*<file>$alias_path</file>#$new_entry#" "$qrc_file" > "$tmp_qrc"
    mv "$tmp_qrc" "$qrc_file"
else
    awk -v entry="$new_entry" '
        BEGIN { in_icons = 0; inserted = 0 }
        /<qresource prefix="\/icons">/ { in_icons = 1 }
        in_icons && /<\/qresource>/ && inserted == 0 {
            print entry
            inserted = 1
            in_icons = 0
        }
        { print }
    ' "$qrc_file" > "$tmp_qrc"
    mv "$tmp_qrc" "$qrc_file"
fi

# Remove legacy direct file if it exists; qrc now points to fluent mirror.
legacy_file="$repo_root/resources/icons/$alias_rel"
removed_legacy=0
if [ -f "$legacy_file" ]; then
    rm -f "$legacy_file"
    removed_legacy=1
fi

echo "Fetched: $rel_path"
echo "Saved: ${dst#$repo_root/}"
echo "qrc alias: $alias_path -> $source_path"
if [ "$removed_legacy" -eq 1 ]; then
    echo "Removed legacy file: ${legacy_file#$repo_root/}"
fi

# Keep derived local icons (like ui/double-checkmark.svg) aligned with their
# Fluent source files when related assets are fetched.
python3 "$repo_root/bin/icons/generate-derived.py"
