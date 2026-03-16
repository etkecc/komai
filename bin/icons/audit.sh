#!/usr/bin/env sh
#
# Audit icon consistency across three sources of truth:
# 1) icon paths referenced by C++/QML code,
# 2) icon aliases/targets registered in resources/res.qrc,
# 3) icon files present under resources/icons/.
#
# The script reports set differences between those lists and exits non-zero on any mismatch:
# - referenced by code but missing in qrc,
# - qrc alias target missing on disk,
# - qrc alias present but not referenced by code,
# - ui/emoji-categories files present on disk but missing in qrc.
# - mirrored Fluent files present on disk but unreferenced by qrc targets.

set -eu
export LC_ALL=C

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo_root"

tmpdir="$(mktemp -d)"
trap 'rm -rf "$tmpdir"' EXIT INT TERM

refs_file="$tmpdir/refs.txt"
qrc_alias_file="$tmpdir/qrc-alias.txt"
qrc_target_file="$tmpdir/qrc-target.txt"
files_file="$tmpdir/files.txt"
ui_emoji_files_file="$tmpdir/ui-emoji-files.txt"
fluent_files_file="$tmpdir/fluent-files.txt"
qrc_fluent_targets_file="$tmpdir/qrc-fluent-targets.txt"

extract_refs() {
    {
        grep -RhoE --binary-files=without-match \
            "icons/(ui|emoji-categories)/[A-Za-z0-9._-]+\\.svg" src resources/qml 2>/dev/null

        grep -RhE --binary-files=without-match \
            'icons/(icons/)?ui/.*\\.svg' src resources/qml 2>/dev/null \
            | grep -oE '[A-Za-z0-9._-]+\.svg' \
            | sed 's#^#icons/ui/#'
    } || true
}

list_svgs() {
    for dir in "$@"; do
        [ -d "$dir" ] || continue
        find "$dir" -type f -name '*.svg'
    done
}

extract_refs \
    | sed 's#^.*icons/##; s#^icons/##' \
    | sort -u > "$refs_file"

awk '
    match($0, /<file alias="([^"]+)">([^<]+)<\/file>/, m) {
        alias = m[1]
        target = m[2]
        if (alias ~ /^icons\/(ui|emoji-categories)\//) {
            sub(/^icons\//, "", alias)
            print alias
            if (target ~ /^icons\//) {
                sub(/^icons\//, "", target)
            }
            print target > target_file
        }
        next
    }
    match($0, /<file>([^<]+)<\/file>/, m) {
        alias = m[1]
        if (alias ~ /^icons\/(ui|emoji-categories)\//) {
            sub(/^icons\//, "", alias)
            print alias
            print alias > target_file
        }
    }
' target_file="$qrc_target_file" resources/res.qrc | sort -u > "$qrc_alias_file"

sort -u -o "$qrc_target_file" "$qrc_target_file"

list_svgs resources/icons \
    | sed 's#^resources/icons/##' \
    | sort -u > "$files_file"

missing_qrc="$tmpdir/missing-qrc.txt"
qrc_only="$tmpdir/qrc-only.txt"
missing_targets="$tmpdir/missing-targets.txt"
files_not_qrc="$tmpdir/ui-emoji-files-not-qrc.txt"
fluent_orphans="$tmpdir/fluent-orphans.txt"

comm -23 "$refs_file" "$qrc_alias_file" > "$missing_qrc"
comm -23 "$qrc_alias_file" "$refs_file" > "$qrc_only"
comm -23 "$qrc_target_file" "$files_file" > "$missing_targets"
list_svgs resources/icons/ui resources/icons/emoji-categories \
    | sed 's#^resources/icons/##' \
    | sort -u > "$ui_emoji_files_file"
comm -23 "$ui_emoji_files_file" "$qrc_alias_file" > "$files_not_qrc"

if [ -d "resources/icons/fluent" ]; then
    list_svgs resources/icons/fluent \
        | sed 's#^resources/icons/##' \
        | sort -u > "$fluent_files_file"
else
    : > "$fluent_files_file"
fi

grep '^fluent/.*\.svg$' "$qrc_target_file" | sort -u > "$qrc_fluent_targets_file" || true
comm -23 "$fluent_files_file" "$qrc_fluent_targets_file" > "$fluent_orphans"

print_section() {
    title="$1"
    file="$2"
    prefix_icons_root="${3:-0}"
    count="$(wc -l < "$file" | tr -d ' ')"
    if [ "$count" -eq 0 ]; then
        printf "✅ %s: %s\n" "$title" "$count"
    else
        printf "⚠️  %s: %s\n" "$title" "$count"
    fi
    if [ "$count" -gt 0 ]; then
        while IFS= read -r line; do
            if [ "$prefix_icons_root" -eq 1 ]; then
                case "$line" in
                    ui/*|emoji-categories/*|fluent/*)
                        line="resources/icons/$line"
                        ;;
                esac
            fi
            escaped="$(printf "%s" "$line" | sed "s/'/'\\\\''/g")"
            printf "  - '%s'\n" "$escaped"
        done < "$file"
    fi
}

print_section "Referenced by code but missing in res.qrc" "$missing_qrc" 0
print_section "res.qrc alias targets missing on disk" "$missing_targets" 1
print_section "Present in res.qrc but not referenced by code" "$qrc_only" 0
print_section "Present on disk (ui/emoji-categories) but missing in res.qrc" "$files_not_qrc" 1
print_section "Mirrored Fluent SVGs on disk but unreferenced in res.qrc" "$fluent_orphans" 1

hard_fail=0
if [ -s "$missing_qrc" ] || [ -s "$missing_targets" ] || [ -s "$qrc_only" ] || [ -s "$files_not_qrc" ]; then
    hard_fail=1
fi
if [ -s "$fluent_orphans" ]; then
    hard_fail=1
fi

echo
if [ "$hard_fail" -eq 0 ]; then
    echo "✅ Icon audit clean."
else
    echo "❌ Icon audit failed."
    exit 1
fi
