#!/usr/bin/env sh

# Deprecated compatibility entry point.

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
exec "$repo_root/bin/license/inject.sh" "$@"
