#!/usr/bin/env sh
# Verifies that the version in VERSION.txt has matching entries across every
# other release-bearing file Komai tracks. Run `just release-prepare` to
# update all surfaces in one go.

set -eu

repo_root="$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)"
version_file="$repo_root/VERSION.txt"
pkgbuild_file="$repo_root/etc/packaging/archlinux/komai/PKGBUILD"
pkgbuild_bin_file="$repo_root/etc/packaging/archlinux/komai-bin/PKGBUILD"
appdata_file="$repo_root/resources/komai.appdata.xml.in"
changelog_file="$repo_root/CHANGELOG.md"

if [ ! -f "$version_file" ]; then
	echo "ERROR: VERSION.txt file not found at $version_file" >&2
	exit 1
fi

version="$(tr -d '[:space:]' < "$version_file")"
if ! printf '%s\n' "$version" | grep -Eq '^[0-9]{4}\.[0-9]{2}\.[0-9]{2}\.[0-9]+$'; then
	echo "ERROR: VERSION.txt is not a valid CalVer (YYYY.MM.DD.N): '$version'" >&2
	exit 1
fi

fail=0

if ! grep -Fxq "pkgver=$version" "$pkgbuild_file"; then
	echo "ERROR: $pkgbuild_file does not declare pkgver=$version" >&2
	fail=1
fi

if ! grep -Fxq "pkgver=$version" "$pkgbuild_bin_file"; then
	echo "ERROR: $pkgbuild_bin_file does not declare pkgver=$version" >&2
	fail=1
fi

# AppStream <release version="X" date="Y"/>; attribute order not fixed.
if ! grep -Eq "<release[^>]*version=\"$version\"" "$appdata_file"; then
	echo "ERROR: $appdata_file has no <release version=\"$version\" .../> entry" >&2
	fail=1
fi

# CHANGELOG heading "## <version>" (any trailing text allowed, e.g. " - 2026-04-20").
if ! grep -Eq "^## $version(\$| )" "$changelog_file"; then
	echo "ERROR: $changelog_file has no '## $version' heading" >&2
	fail=1
fi

if [ "$fail" -ne 0 ]; then
	echo >&2
	echo "Run 'just release-prepare' (optionally with an explicit version) to" >&2
	echo "bump VERSION.txt and refresh every drift surface in one go." >&2
fi

exit "$fail"
