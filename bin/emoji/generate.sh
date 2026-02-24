#!/usr/bin/env sh

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
cd "$repo_root"

cat resources/provider-head.txt > src/emoji/Provider.cpp
cat resources/extra_emoji.txt resources/emoji-test.txt > resources/complete-emoji.txt
bin/emoji/codegen.py impl resources/complete-emoji.txt resources/shortcodes.txt >> src/emoji/Provider.cpp
bin/emoji/codegen.py header resources/complete-emoji.txt resources/shortcodes.txt > src/emoji/Provider.h
