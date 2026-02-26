#!/usr/bin/env sh

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"

# Some sandboxed environments provide non-writable XDG runtime dirs (e.g. /run/user/*).
# Force just's temp script directory into /tmp for this hook run.
JUST_TEMPDIR="${JUST_TEMPDIR:-/tmp/just}" \
just --justfile "$repo_root/justfile" test
