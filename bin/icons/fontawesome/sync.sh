#!/usr/bin/env sh
#
# Sync all mirrored Font Awesome icons from pinned upstream.
#
# Inputs:
# - bin/icons/fontawesome/repo.conf (FA_REPO pin)
# - bin/icons/fontawesome/VERSION_REF (pinned version/tag/commit ref)
#
# This script is a maintenance tool (not part of normal build flow).
# It downloads all SVGs currently present under resources/icons/fontawesome/
# by reusing their relative path as the upstream path.
# Use --dry-run to preview planned fetches.

set -eu

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH= cd -- "$script_dir/../../.." && pwd)"
cd "$repo_root"

lock_file="$script_dir/repo.conf"
version_ref_file="$script_dir/VERSION_REF"
fa_root="$repo_root/resources/icons/fontawesome"

dry_run=0
if [ "${1:-}" = "--dry-run" ]; then
    dry_run=1
fi

if [ ! -f "$lock_file" ]; then
    echo "Missing config file: $lock_file" >&2
    exit 1
fi

if [ ! -f "$version_ref_file" ]; then
    echo "Missing version ref file: $version_ref_file" >&2
    exit 1
fi
raw_ref="$(awk 'NF > 0 && $1 !~ /^#/ { print $1; exit }' "$version_ref_file")"
if [ -z "$raw_ref" ]; then
    echo "Set a version/ref in bin/icons/fontawesome/VERSION_REF first." >&2
    exit 1
fi

# shellcheck source=/dev/null
. "$lock_file"

if [ -z "${FA_REPO:-}" ]; then
    echo "Set FA_REPO in bin/icons/fontawesome/repo.conf first." >&2
    exit 1
fi

FA_REF="$raw_ref"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT INT TERM

if [ ! -d "$fa_root" ]; then
    echo "No Font Awesome mirror directory yet: $fa_root"
    echo "Fetch icons first with: just icons-fetch-fontawesome <style> <name>"
    exit 0
fi

found=0
updated=0
list_file="$tmpdir/fa-files.txt"
find "$fa_root" -type f -name '*.svg' -print0 > "$list_file"

if [ -s "$list_file" ]; then
    found=1
fi

while IFS= read -r -d '' dst; do
    rel_path="${dst#$fa_root/}"
    url="https://raw.githubusercontent.com/$FA_REPO/$FA_REF/$rel_path"
    if [ "$dry_run" -eq 1 ]; then
        echo "Would sync: resources/icons/fontawesome/$rel_path <- $url"
        continue
    fi

    tmp="$tmpdir/icon.svg"
    curl -fsSL "$url" -o "$tmp"
    mv "$tmp" "$dst"
    updated=$((updated + 1))
    echo "Synced: resources/icons/fontawesome/$rel_path"
done < "$list_file"

if [ "$found" -eq 0 ]; then
    echo "No mirrored Font Awesome SVG files found under resources/icons/fontawesome/."
    echo "Fetch icons first with: just icons-fetch-fontawesome <style> <name>"
    exit 0
fi

if [ "$dry_run" -eq 1 ]; then
    exit 0
fi

echo "Updated $updated icon(s)."
