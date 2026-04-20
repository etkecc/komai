#!/usr/bin/env sh
# Verifies the committed shell completion files under resources/completions/
# match what `komai completions <shell>` currently emits. Regenerate them
# with `just completions-generate` if this script fails.

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
build_dir="$repo_root/var/build/native"

just --justfile "$repo_root/justfile" build >/dev/null

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

fail=0
for entry in "bash bash/komai" "zsh zsh/_komai" "fish fish/komai.fish"; do
	# shellcheck disable=SC2086
	set -- $entry
	shell="$1"
	path="$2"

	"$build_dir/komai" completions "$shell" >"$tmp/$shell"
	if ! diff -u "$repo_root/resources/completions/$path" "$tmp/$shell"; then
		echo >&2
		echo "ERROR: resources/completions/$path is out of date." >&2
		echo "       Regenerate with: just completions-generate" >&2
		fail=1
	fi
done

exit "$fail"
