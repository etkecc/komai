#!/usr/bin/env bash

# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -euo pipefail

repo_root="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$repo_root"

violations=0

check_pattern() {
    local pattern="$1"
    local description="$2"

    local matches
    matches="$(rg -n "$pattern" src/db || true)"
    if [[ -n "$matches" ]]; then
        echo "db boundary violation: $description"
        echo "$matches"
        echo
        violations=1
    fi
}

check_pattern '^#include "cache/' 'src/db must not include cache headers'
check_pattern '^#include "timeline/' 'src/db must not include timeline headers'
check_pattern '^#include "encryption/' 'src/db must not include encryption headers'
check_pattern '^#include "ui/' 'src/db must not include UI headers'

allowed_matrix_state_files=(
    'src/db/MemberInfo.cpp'
    'src/db/RoomInfo.cpp'
)

is_allowed_matrix_state_file() {
    local candidate="$1"
    for allowed in "${allowed_matrix_state_files[@]}"; do
        if [[ "$candidate" == "$allowed" ]]; then
            return 0
        fi
    done

    return 1
}

while IFS= read -r entry; do
    [[ -z "$entry" ]] && continue

    file="${entry%%:*}"
    if ! is_allowed_matrix_state_file "$file"; then
        if [[ "$violations" -eq 0 ]]; then
            echo 'db boundary violation: MatrixStateTypes.h is allowed only in the db adapter files:'
            printf ' - %s\n' "${allowed_matrix_state_files[@]}"
        fi
        echo "$entry"
        violations=1
    fi
done < <(rg -n '^#include "MatrixStateTypes\.h"' src/db || true)

if [[ "$violations" -ne 0 ]]; then
    echo
    echo 'See docs/architecture/storage.md and docs/architecture/cache/README.md for module boundaries.'
    exit 1
fi

echo 'db boundary check: ok'
