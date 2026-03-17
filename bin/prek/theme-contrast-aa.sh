#!/usr/bin/env sh

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

python3 "$repo_root/bin/theme/contrast.py" --fail-aa
