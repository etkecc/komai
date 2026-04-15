#!/usr/bin/env sh
#
# Sync all mirrored Fluent icons from pinned upstream.
#
# Inputs:
# - bin/icons/fluent/repo.conf (FLUENT_REPO pin)
# - bin/icons/fluent/VERSION_REF (pinned version/tag/commit ref)
#
# This script is a maintenance tool (not part of normal build flow).
# It downloads all SVGs currently present under resources/icons/fluent/
# by reusing their relative path as the upstream path.
# Use --dry-run to preview planned fetches.

set -eu

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH= cd -- "$script_dir/../../.." && pwd)"
cd "$repo_root"

lock_file="$script_dir/repo.conf"
version_ref_file="$script_dir/VERSION_REF"
fluent_root="$repo_root/resources/icons/fluent"

dry_run=0
if [ "${1:-}" = "--dry-run" ]; then
    dry_run=1
fi

if [ ! -f "$lock_file" ]; then
    echo "Missing lock file: $lock_file" >&2
    exit 1
fi

# first non-comment non-empty line
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

FLUENT_REF="$raw_ref"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT INT TERM

if [ ! -d "$fluent_root" ]; then
    echo "Missing Fluent mirror directory: $fluent_root" >&2
    exit 1
fi

found=0
updated=0
list_file="$tmpdir/fluent-files.txt"
find "$fluent_root" -type f -name '*.svg' -print0 > "$list_file"

if [ -s "$list_file" ]; then
    found=1
fi

while IFS= read -r -d '' dst; do
    rel_path="${dst#$fluent_root/}"
    url_rel_path="$(python3 - "$rel_path" <<'PY'
import sys
import urllib.parse

print(urllib.parse.quote(sys.argv[1], safe="/-._~%"))
PY
)"
    url="https://raw.githubusercontent.com/$FLUENT_REPO/$FLUENT_REF/$url_rel_path"
    if [ "$dry_run" -eq 1 ]; then
        echo "Would sync: resources/icons/fluent/$rel_path <- $url"
        continue
    fi

    tmp="$tmpdir/icon.svg"
    curl -fsSL "$url" -o "$tmp"
    mv "$tmp" "$dst"
    updated=$((updated + 1))
    echo "Synced: resources/icons/fluent/$rel_path"
done < "$list_file"

if [ "$found" -eq 0 ]; then
    echo "No mirrored Fluent SVG files found under resources/icons/fluent/."
    echo "Add mirrored files first, then run icons-sync."
    exit 0
fi

if [ "$dry_run" -eq 1 ]; then
    python3 "$repo_root/bin/icons/generate-derived.py" --dry-run
    exit 0
fi

echo "Updated $updated icon(s)."

# Keep derived local icons in sync with their Fluent source files.
python3 "$repo_root/bin/icons/generate-derived.py"
