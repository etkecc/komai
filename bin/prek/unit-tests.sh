#!/usr/bin/env sh

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
build_dir="$repo_root/var/build/native"

if [ ! -f "$build_dir/CMakeCache.txt" ]; then
    cmake -S "$repo_root" -B "$build_dir" -DCMAKE_BUILD_TYPE=Release -DMAN=OFF
fi

cmake --build "$build_dir" --parallel "$(nproc)" --target komai_tests
ctest --test-dir "$build_dir" --output-on-failure
