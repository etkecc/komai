set tempdir := "var/tmp/just"

# Paths
build_dir := justfile_directory() / "var/build/native"
flatpak_build_dir := justfile_directory() / "var/build/flatpak"
appimage_build_dir := justfile_directory() / "var/build/appimage"
snap_build_dir := justfile_directory() / "var/build/snap"
static_web_server_container_image := "ghcr.io/static-web-server/static-web-server:2.41.0"

# mise (dev tool version manager)
mise_data_dir := env("MISE_DATA_DIR", justfile_directory() / "var/mise")
mise_trusted_config_paths := justfile_directory() / "mise.toml"
prek_home := env("PREK_HOME", justfile_directory() / "var/prek")

# Shows help
default:
	@just --list --justfile {{ justfile() }}

# Configures the build (CMake configure step)
configure *args:
	bash {{ justfile_directory() }}/bin/build/native.sh configure {{ args }}

# Builds the project (configures first if needed)
build *args: _ensure_just_temp_directory
	bash {{ justfile_directory() }}/bin/build/native.sh build {{ args }}

# Regenerates the committed shell completion files in resources/completions/
completions-generate: build
	{{ build_dir }}/komai completions bash > {{ justfile_directory() }}/resources/completions/bash/komai
	{{ build_dir }}/komai completions zsh  > {{ justfile_directory() }}/resources/completions/zsh/_komai
	{{ build_dir }}/komai completions fish > {{ justfile_directory() }}/resources/completions/fish/komai.fish

# Prepares a new release: bumps VERSION.txt and propagates to PKGBUILD/CHANGELOG/appdata. Without args, picks the next version from today's UTC date.
release-prepare *args:
	python3 {{ justfile_directory() }}/bin/release/prepare.py {{ args }}

# Local fallback for publish.yml (step 1 of release-manual-all): validates publish prerequisites for v<VERSION.txt> (clean tree, tag, CHANGELOG, drift, gh auth, no existing release).
release-manual-validate:
	python3 {{ justfile_directory() }}/bin/release/validate.py

# Local fallback for publish.yml (step 2 of release-manual-all): builds AppImage, Flatpak, and Snap artefacts for v<VERSION.txt>.
release-manual-build:
	python3 {{ justfile_directory() }}/bin/release/build.py

# Local fallback for publish.yml (step 3 of release-manual-all): publishes the built artefacts to a GitHub Release; hard-fails if one already exists for the tag. Supports `--dry-run`.
release-manual-publish *args:
	python3 {{ justfile_directory() }}/bin/release/publish.py {{ args }}

# Local fallback for publish.yml: end-to-end release for v<VERSION.txt> (validate, build, publish). Run `release-prepare` and tag first. Supports `--dry-run`.
release-manual-all *args:
	python3 {{ justfile_directory() }}/bin/release/all.py {{ args }}

# Runs the full supported test suite (C++ unit + integration, then Rust unit tests)
test: _ensure_just_temp_directory
	just --justfile {{ justfile() }} test-cpp
	just --justfile {{ justfile() }} test-rust-unit

# Runs all C++/CTest tests
test-cpp *args: _ensure_just_temp_directory
	bash {{ justfile_directory() }}/bin/build/native.sh test-cpp {{ args }}

# Runs C++/CTest unit tests
test-cpp-unit *args: _ensure_just_temp_directory
	bash {{ justfile_directory() }}/bin/build/native.sh test-cpp-unit {{ args }}

# Runs C++/CTest integration tests
test-cpp-integration *args: _ensure_just_temp_directory
	bash {{ justfile_directory() }}/bin/build/native.sh test-cpp-integration {{ args }}

# Runs Rust unit tests
test-rust-unit *args:
	cargo test --manifest-path {{ justfile_directory() }}/src/rust/Cargo.toml --lib {{ args }}

# Backward-compatible aliases
test-unit *args: _ensure_just_temp_directory
	just --justfile {{ justfile() }} test-cpp-unit {{ args }}

test-integration *args: _ensure_just_temp_directory
	just --justfile {{ justfile() }} test-cpp-integration {{ args }}

# Configures and builds from scratch
rebuild *args:
	bash {{ justfile_directory() }}/bin/build/native.sh rebuild {{ args }}

# Installs the compiled binary (may require sudo; prompts before writing to /usr, /opt)
install:
	bash {{ justfile_directory() }}/bin/build/native.sh install

# Runs the compiled binary (builds first if needed)
run *args: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	binary="{{ build_dir }}/komai"
	if [[ ! -x "$binary" ]]; then
		just --justfile {{ justfile() }} build
	fi
	exec "$binary" {{ args }}

# Runs Komai with room-switch performance tracing enabled
run-with-perf-trace *args: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	binary="{{ build_dir }}/komai"
	if [[ ! -x "$binary" ]]; then
		just --justfile {{ justfile() }} build
	fi
	export KOMAI_ROOM_SWITCH_PERF=1
	if [[ -z "${KOMAI_LOG_LEVEL:-}" ]]; then
		export KOMAI_LOG_LEVEL="info,ui=info"
	fi
	if [[ -z "${KOMAI_LOG_TYPE:-}" ]]; then
		export KOMAI_LOG_TYPE="file,stderr"
	else
		case ",${KOMAI_LOG_TYPE}," in
			*,file,*) ;;
			*) export KOMAI_LOG_TYPE="${KOMAI_LOG_TYPE},file" ;;
		esac
		case ",${KOMAI_LOG_TYPE}," in
			*,stderr,*) ;;
			*) export KOMAI_LOG_TYPE="${KOMAI_LOG_TYPE},stderr" ;;
		esac
	fi
	exec "$binary" {{ args }}

# Reports theme contrast ratios (all themes by default, or pass one/more slugs)
theme-check-contrast *themes:
	python3 {{ justfile_directory() }}/bin/theme/contrast.py {{ themes }}

# Same as theme-check-contrast, but exits non-zero on hard AA failures
theme-check-contrast-strict *themes:
	python3 {{ justfile_directory() }}/bin/theme/contrast.py --fail-aa {{ themes }}

# Serves the theme preview SPA with built-in themes mounted at /resources/themes/
theme-preview-run port='20680':
	#!/usr/bin/env bash
	set -euo pipefail
	echo "Starting theme preview: http://127.0.0.1:{{ port }}"
	/usr/bin/env docker run \
		-it \
		--user="$(id -u):$(id -g)" \
		--read-only \
		--cap-drop=ALL \
		--rm \
		-e SERVER_PORT=8080 \
		-e SERVER_ROOT=/srv \
		-e SERVER_DIRECTORY_LISTING=true \
		-e SERVER_DIRECTORY_LISTING_FORMAT=json \
		-p 127.0.0.1:{{ port }}:8080 \
		--mount type=bind,src={{ justfile_directory() }}/etc/tools/theme-preview,dst=/srv,ro \
		--mount type=bind,src={{ justfile_directory() }}/resources/themes,dst=/srv/resources/themes,ro \
		{{ static_web_server_container_image }}

# Imports a tinted-theming Base16 theme into resources/themes/ (builds first if needed)
theme-tinted-import slug *args: build
	#!/usr/bin/env bash
	set -euo pipefail
	output=$(just --justfile {{ justfile() }} run theme tinted-import {{ slug }} --force {{ args }} 2>&1)
	echo "$output"
	saved_path=$(echo "$output" | grep -oP '(?<=Theme saved to: ).*\.yml$')
	if [[ -z "$saved_path" || ! -f "$saved_path" ]]; then
		echo "ERROR: Could not determine saved theme path" >&2
		exit 1
	fi
	mv "$saved_path" "{{ justfile_directory() }}/resources/themes/"
	echo "Relocated: $(basename "$saved_path") → resources/themes/"

# Creates a starter theme YAML in resources/themes/ (builds first if needed)
theme-create-sample variant name *args: build
	#!/usr/bin/env bash
	set -euo pipefail
	output=$(just --justfile {{ justfile() }} run theme create-sample {{ variant }} {{ name }} {{ args }} 2>&1)
	echo "$output"
	saved_path=$(echo "$output" | grep -oP '(?<=Theme saved to: ).*\.yml$')
	if [[ -z "$saved_path" || ! -f "$saved_path" ]]; then
		echo "ERROR: Could not determine saved theme path" >&2
		exit 1
	fi
	mv "$saved_path" "{{ justfile_directory() }}/resources/themes/"
	echo "Relocated: $(basename "$saved_path") → resources/themes/"

# Fetches pinned Unicode/CLDR emoji sources into var/emoji/cache/
emoji-fetch:
	python3 {{ justfile_directory() }}/bin/emoji/pipeline.py fetch --repo-root {{ justfile_directory() }}

# Builds runtime emoji JSON data from cached/fetched sources
emoji-build:
	python3 {{ justfile_directory() }}/bin/emoji/pipeline.py build --repo-root {{ justfile_directory() }}

# Validates emoji lock/overrides and cache-based reproducibility
emoji-check:
	python3 {{ justfile_directory() }}/bin/emoji/pipeline.py check --repo-root {{ justfile_directory() }}

# Re-pins the CLDR tarball sha256s in bin/emoji/sources.lock.yml (use after a Renovate version bump)
emoji-update-lock:
	python3 {{ justfile_directory() }}/bin/emoji/pipeline.py update-lock --repo-root {{ justfile_directory() }}

# Adds one locale-specific emoji search token override entry
emoji-add-token emoji locale token:
	python3 {{ justfile_directory() }}/bin/emoji/pipeline.py add-token "{{ emoji }}" "{{ locale }}" "{{ token }}" --repo-root {{ justfile_directory() }}

# Backward-compatible alias (deprecated; prefer emoji-build)
emoji-generate:
	just --justfile {{ justfile() }} emoji-build

# Fetches the pinned Element Call embedded bundle into var/element-call/<version>/
# (used directly by CMake; run explicitly to pre-populate for offline packaging builds)
element-call-fetch:
	python3 {{ justfile_directory() }}/bin/element-call/fetch.py --lock {{ justfile_directory() }}/bin/element-call/sources.lock.yml --out-dir {{ justfile_directory() }}/var/element-call

# Re-pins sources.lock.yml's sha256 to the current version's real tarball hash
# (run after a Renovate version bump: Renovate updates version but not the hash)
element-call-update-lock:
	python3 {{ justfile_directory() }}/bin/element-call/fetch.py --update-lock --lock {{ justfile_directory() }}/bin/element-call/sources.lock.yml --out-dir {{ justfile_directory() }}/var/element-call

# Audits icon references vs resources/res.qrc and files on disk
icons-audit *args:
	{{ justfile_directory() }}/bin/icons/audit.sh {{ args }}

# Regenerates docs/architecture/icons-list.md from resources/res.qrc
icons-generate-list *args:
	python3 {{ justfile_directory() }}/bin/icons/generate-list.py {{ args }}

# Regenerates derived local icons from Fluent source assets
icons-generate-derived *args:
	python3 {{ justfile_directory() }}/bin/icons/generate-derived.py {{ args }}

# Syncs mirrored Fluent icons from pinned source
icons-sync-fluent *args:
	{{ justfile_directory() }}/bin/icons/fluent/sync.sh {{ args }}

# Fetches one Fluent icon  (e.g. rel_path = assets/Something/something.svg) into resources/icons/fluent/ and wires a qrc alias
icons-fetch-fluent rel_path alias_svg_name:
	{{ justfile_directory() }}/bin/icons/fluent/fetch.sh "{{ rel_path }}" "{{ alias_svg_name }}"

# Syncs mirrored Font Awesome icons from pinned source
icons-sync-fontawesome *args:
	{{ justfile_directory() }}/bin/icons/fontawesome/sync.sh {{ args }}

# Fetches one Font Awesome icon (e.g. rel_path = svgs/solid/hammer.svg) into resources/icons/fontawesome/ and wires a qrc alias
icons-fetch-fontawesome rel_path alias_svg_name:
	{{ justfile_directory() }}/bin/icons/fontawesome/fetch.sh "{{ rel_path }}" "{{ alias_svg_name }}"

# Removes the build directory
clean:
	bash {{ justfile_directory() }}/bin/build/native.sh clean

# Configures a debug build
configure-debug *args:
	bash {{ justfile_directory() }}/bin/build/native.sh configure-debug {{ args }}

# Extracts translatable strings from source code into .ts files, then normalizes
translations-update: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	ts_files=()
	for f in {{ justfile_directory() }}/resources/langs/*/komai_*.ts; do
		ts_files+=("$f")
	done
	/usr/lib/qt6/bin/lupdate \
		-locations relative \
		-extensions java,jui,ui,c,c++,cc,cpp,cxx,ch,h,h++,hh,hpp,hxx,inc,js,mjs,qs,qml,qrc \
		{{ justfile_directory() }}/src/ \
		{{ justfile_directory() }}/resources/qml/ \
		-ts "${ts_files[@]}" \
		-no-obsolete
	just --justfile {{ justfile() }} translations-normalize

# Normalizes .ts files to a canonical XML format (idempotent)
translations-normalize *args:
	python3 {{ justfile_directory() }}/bin/translations/translate.py normalize {{ args }}

# Canonicalizes inconsistent translations: same source -> single most-frequent translation per language
translations-canonicalize *args:
	python3 {{ justfile_directory() }}/bin/translations/normalize-inconsistencies.py {{ args }}

# Auto-translates unfinished strings for a language using Claude CLI
translations-claude-translate-lang lang *args:
	python3 {{ justfile_directory() }}/bin/translations/translate.py translate {{ lang }} {{ args }}

# Override concurrency with `PARALLELISM=10 just translations-claude-translate-all`.
# Auto-translates unfinished strings for all languages using Claude CLI (default: 5 concurrent)
translations-claude-translate-all *args: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	parallelism="${PARALLELISM:-5}"
	langs=()
	for d in {{ justfile_directory() }}/resources/langs/*/; do
		lang=$(basename "$d")
		[[ "$lang" == "en" ]] && continue
		[[ -f "$d/komai_${lang}.ts" ]] || continue
		langs+=("$lang")
	done
	printf '%s\n' "${langs[@]}" \
		| xargs -P "$parallelism" -I _LANG_ bash -c '
			lang=_LANG_
			{ just --justfile "{{ justfile() }}" translations-claude-translate-lang "$lang" "$@" 2>&1 \
				|| echo "ERROR: Translation failed for $lang, continuing..."; } \
				| sed "s|^|[$lang] |"
		' _ {{ args }}

# Checks Markdown links for local path validity
docs-check-links:
	python3 {{ justfile_directory() }}/bin/docs/check-links.py

# Summarizes room-switch performance markers from a log file
perf-room-switch-report logfile:
	python3 {{ justfile_directory() }}/bin/perf/room_switch_report.py "{{ logfile }}"

# Summarizes room-switch performance markers for a profile (default: default)
perf-room-switch-report-profile profile="default":
	python3 {{ justfile_directory() }}/bin/perf/room_switch_report.py --profile "{{ profile }}"

# Regenerates etc/packaging/flatpak/cargo-sources.json from src/rust/Cargo.lock
flatpak-cargo-sources:
	python3 {{ justfile_directory() }}/bin/flatpak/cargo-sources.py {{ justfile_directory() }}

# Builds a Flatpak bundle from the local source tree
flatpak-build: _ensure_just_temp_directory emoji-fetch element-call-fetch flatpak-cargo-sources
	#!/usr/bin/env bash
	set -euo pipefail

	# flatpak-builder skips .git (see cc.etke.komai.yaml), so CMake inside
	# the sandbox can't probe the commit hash.  Write it to a file that
	# CMakeLists.txt reads as a fallback — only the value is shipped, no
	# git history exposure.
	if git -C "{{ justfile_directory() }}" rev-parse --short=8 HEAD >"{{ justfile_directory() }}/.git-commit-hash" 2>/dev/null; then
		echo "Wrote .git-commit-hash: $(cat "{{ justfile_directory() }}/.git-commit-hash")"
	else
		rm -f "{{ justfile_directory() }}/.git-commit-hash"
	fi

	# Ensure flathub is available as a user remote (needed for --install-deps-from)
	if ! flatpak remotes --user --columns=name | grep -qx flathub; then
		echo "Adding flathub user remote..."
		flatpak remote-add --user --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo
	fi

	mkdir -p "{{ flatpak_build_dir }}"
	flatpak-builder \
		--install-deps-from=flathub \
		--user \
		--disable-rofiles-fuse \
		--ccache \
		--state-dir="{{ flatpak_build_dir }}/.flatpak-builder" \
		--repo="{{ flatpak_build_dir }}/repo" \
		--force-clean \
		"{{ flatpak_build_dir }}/app" \
		"{{ justfile_directory() }}/etc/packaging/flatpak/cc.etke.komai.yaml"
	# Filename mirrors the AppImage style (komai-<version>-<arch>.<ext>) so
	# users keeping older bundles can tell them apart. flatpak build-bundle
	# defaults to the host arch, matching the build above.
	komai_version="$(tr -d '[:space:]' < "{{ justfile_directory() }}/VERSION.txt")"
	host_arch="$(uname -m)"
	bundle_path="{{ flatpak_build_dir }}/komai-${komai_version}-${host_arch}.flatpak"
	rm -f "{{ flatpak_build_dir }}"/komai-*.flatpak
	flatpak build-bundle \
		"{{ flatpak_build_dir }}/repo" \
		"$bundle_path" \
		cc.etke.komai
	echo "Flatpak bundle: $bundle_path"

# Installs the locally-built Flatpak bundle
flatpak-install:
	flatpak --user install --or-update -y "{{ flatpak_build_dir }}"/komai-*.flatpak

# Runs the Flatpak-installed Komai
flatpak-run *args:
	flatpak run cc.etke.komai {{ args }}

# Removes the Flatpak build directory
flatpak-clean:
	rm -rf "{{ flatpak_build_dir }}"

# Builds an AppImage bundle inside a Docker container (works on any distro)
appimage-build-docker: _ensure_just_temp_directory emoji-fetch element-call-fetch
	#!/usr/bin/env bash
	set -euo pipefail

	# The Docker build copies the source tree into a container where git's
	# safe.directory check may reject the repo (bind-mount ownership
	# mismatch), causing CMake's git probe to fail and the hash to land
	# as "unknown".  Write the hash on the host so CMake has a fallback.
	if git -C "{{ justfile_directory() }}" rev-parse --short=8 HEAD >"{{ justfile_directory() }}/.git-commit-hash" 2>/dev/null; then
		echo "Wrote .git-commit-hash: $(cat "{{ justfile_directory() }}/.git-commit-hash")"
	else
		rm -f "{{ justfile_directory() }}/.git-commit-hash"
	fi

	"{{ justfile_directory() }}/etc/packaging/appimage/bin/build-docker" "{{ justfile_directory() }}" "{{ appimage_build_dir }}"

# Builds an AppImage bundle natively (requires Ubuntu 25.04+ with appimage-builder installed)
appimage-build-native: emoji-fetch element-call-fetch
	{{ justfile_directory() }}/etc/packaging/appimage/bin/build-native "{{ justfile_directory() }}" "{{ appimage_build_dir }}"

# Builds a snap package inside a Docker container (works on any distro)
snap-build-docker: _ensure_just_temp_directory emoji-fetch element-call-fetch
	#!/usr/bin/env bash
	set -euo pipefail

	# Snap build-docker tars the source with `--exclude=.git`, so CMake
	# inside the container has nothing to probe.  Write the hash on the
	# host so CMake reads it as a fallback.
	if git -C "{{ justfile_directory() }}" rev-parse --short=8 HEAD >"{{ justfile_directory() }}/.git-commit-hash" 2>/dev/null; then
		echo "Wrote .git-commit-hash: $(cat "{{ justfile_directory() }}/.git-commit-hash")"
	else
		rm -f "{{ justfile_directory() }}/.git-commit-hash"
	fi

	"{{ justfile_directory() }}/etc/packaging/snap/bin/build-docker" "{{ justfile_directory() }}" "{{ snap_build_dir }}"

# Builds a snap package natively (requires snapcraft + LXD)
snap-build-native: emoji-fetch element-call-fetch
	{{ justfile_directory() }}/etc/packaging/snap/bin/build-native "{{ justfile_directory() }}" "{{ snap_build_dir }}"

# Builds a snap package natively in destructive mode (no LXD, builds directly on host)
snap-build-native-destructive: emoji-fetch element-call-fetch
	SNAP_DESTRUCTIVE_MODE=1 {{ justfile_directory() }}/etc/packaging/snap/bin/build-native "{{ justfile_directory() }}" "{{ snap_build_dir }}"

# Installs the locally-built snap (--dangerous for unsigned local snaps)
snap-install:
	sudo snap install --dangerous "{{ snap_build_dir }}"/komai_*.snap

# Runs the snap-installed Komai
snap-run *args:
	snap run komai {{ args }}

# Removes the snap build directory (uses Docker if needed for root-owned files)
snap-clean: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	if [[ ! -e "{{ snap_build_dir }}" ]]; then
		exit 0
	fi
	image="$(tr -d '[:space:]' < "{{ justfile_directory() }}/etc/packaging/snap/builder-image")"
	rm -rf "{{ snap_build_dir }}" 2>/dev/null || \
		docker run --rm -v "{{ justfile_directory() }}/var/build:/cleanup" --entrypoint bash "$image" -c "rm -rf /cleanup/snap"

# Removes the AppImage build directory (uses Docker if needed for root-owned files)
appimage-clean: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	if [[ ! -e "{{ appimage_build_dir }}" ]]; then
		exit 0
	fi
	image="$(tr -d '[:space:]' < "{{ justfile_directory() }}/etc/packaging/appimage/builder-image")"
	rm -rf "{{ appimage_build_dir }}" 2>/dev/null || \
		docker run --rm -v "{{ justfile_directory() }}/var/build:/cleanup" "$image" rm -rf /cleanup/appimage

# Runs selected lint/format hooks via prek
lint:
	@just --justfile {{ justfile() }} prek-run-on-all \
		trailing-whitespace \
		end-of-file-fixer \
		check-yaml \
		check-json \
		clang-format \
		check-serverlist \
		check-theme-yaml \
		builtin-theme-wcag-aa \
		check-ts-normalized \
		qmllint \
		emoji-check \
		icons-audit \
		icons-list-check \
			icons-derived-check \
			check-markdown-links \
			no-qsettings \
			db-boundary \
			license-check

# Runs REUSE compliance lint (skips if reuse is unavailable in this environment)
license-check:
	{{ justfile_directory() }}/bin/license/check.sh

# Injects SPDX headers into source files that are currently missing them
license-inject:
	{{ justfile_directory() }}/bin/license/inject.sh

# Backward-compatible alias
license:
	@just --justfile {{ justfile() }} license-check

# Invokes mise with the project-local data directory
mise *args: _ensure_mise_data_directory
	#!/bin/sh
	export MISE_DATA_DIR="{{ mise_data_dir }}"
	export MISE_TRUSTED_CONFIG_PATHS="{{ mise_trusted_config_paths }}"
	export MISE_YES=1
	export PREK_HOME="{{ prek_home }}"
	mise {{ args }}

# Runs prek (pre-commit hooks manager) with the given arguments
prek *args: _ensure_mise_tools_installed
	@just --justfile {{ justfile() }} mise exec -- prek {{ args }}

# Runs pre-commit hooks on staged files
prek-run-on-staged *args: _ensure_mise_tools_installed
	@just --justfile {{ justfile() }} mise exec -- prek run {{ args }}

# Runs pre-commit hooks on all files
prek-run-on-all *args: _ensure_mise_tools_installed
	@just --justfile {{ justfile() }} mise exec -- prek run --all-files {{ args }}

# Installs the git pre-commit hook (runs prek automatically before each commit)
prek-install-git-pre-commit-hook: _ensure_mise_tools_installed
	#!/usr/bin/env sh
	set -eu
	just --justfile {{ justfile() }} mise exec -- prek install
	# The installed git hooks run later under Git, outside this just/mise environment,
	# so they need to be told how to find their tooling:
	#
	# - PREK_HOME keeps prek's cache under var/prek instead of a global home dir.
	# - MISE_DATA_DIR / MISE_TRUSTED_CONFIG_PATHS make mise resolve against this project's
	#   own data directory. Without them mise falls back to the global one and silently
	#   installs a second copy of the tool there.
	# - prek bakes the full path of the currently installed version into the hook
	#   (var/mise/installs/prek/<version>/...), which stops working as soon as the pinned
	#   version changes or old versions are pruned. Pointing at mise's shim instead makes
	#   the hook resolve whatever mise.toml pins, at the time it runs.
	#
	# Which hook files prek installs depends on `default_install_hook_types` in
	# .pre-commit-config.yaml, so patch every hook file that prek generated.
	for hook in "{{ justfile_directory() }}"/.git/hooks/*; do
		[ -f "$hook" ] || continue
		grep -q 'generated by prek' "$hook" || continue
		grep -q '^export PREK_HOME=' "$hook" || sed -i '2iexport PREK_HOME="{{ prek_home }}"' "$hook"
		grep -q '^export MISE_DATA_DIR=' "$hook" || sed -i '3iexport MISE_DATA_DIR="{{ mise_data_dir }}"' "$hook"
		grep -q '^export MISE_TRUSTED_CONFIG_PATHS=' "$hook" || sed -i '4iexport MISE_TRUSTED_CONFIG_PATHS="{{ mise_trusted_config_paths }}"' "$hook"
		sed -i 's#^PREK=".*"$#PREK="{{ mise_data_dir }}/shims/prek"#' "$hook"
	done

# Internal - ensures var/mise directory exists
_ensure_just_temp_directory:
	@mkdir -p "{{ justfile_directory() }}/var/tmp/just"

# Internal - ensures var/mise directory exists
_ensure_mise_data_directory: _ensure_just_temp_directory
	@mkdir -p "{{ mise_data_dir }}"
	@mkdir -p "{{ prek_home }}"

# Internal - ensures mise tools are installed
_ensure_mise_tools_installed: _ensure_mise_data_directory
	@just --justfile {{ justfile() }} mise install --quiet
