set tempdir := "var/tmp/just"

# Paths
build_dir := justfile_directory() / "var/build/native"
flatpak_build_dir := justfile_directory() / "var/build/flatpak"
appimage_build_dir := justfile_directory() / "var/build/appimage"
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
	cmake -S {{ justfile_directory() }} -B {{ build_dir }} \
		-DCMAKE_BUILD_TYPE=Release \
		-DMAN=OFF \
		{{ args }}

# Builds the project (configures first if needed)
build *args: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	if [[ ! -f "{{ build_dir }}/CMakeCache.txt" ]] || grep -q '^USE_BUNDLED_MTXCLIENT:BOOL=OFF$' "{{ build_dir }}/CMakeCache.txt"; then
		just --justfile {{ justfile() }} configure
	fi
	cmake --build {{ build_dir }} --parallel "$(nproc)" {{ args }}

# Runs all tests
test: test-unit test-integration

# Runs unit tests
test-unit *args: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	if [[ ! -f "{{ build_dir }}/CMakeCache.txt" ]] || grep -q '^USE_BUNDLED_MTXCLIENT:BOOL=OFF$' "{{ build_dir }}/CMakeCache.txt"; then
		just --justfile {{ justfile() }} configure
	fi
	cmake --build {{ build_dir }} --parallel "$(nproc)" --target komai_tests
	ctest --test-dir {{ build_dir }} --output-on-failure -L unit {{ args }}

# Runs integration tests
test-integration *args: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	if [[ ! -f "{{ build_dir }}/CMakeCache.txt" ]] || grep -q '^USE_BUNDLED_MTXCLIENT:BOOL=OFF$' "{{ build_dir }}/CMakeCache.txt"; then
		just --justfile {{ justfile() }} configure
	fi
	cmake --build {{ build_dir }} --parallel "$(nproc)" --target komai_tests
	ctest --test-dir {{ build_dir }} --output-on-failure -L integration {{ args }}

# Configures and builds from scratch
rebuild *args:
	just --justfile {{ justfile() }} configure {{ args }}
	just --justfile {{ justfile() }} build

# Installs the compiled binary (may require sudo)
install:
	cmake --install {{ build_dir }}

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

# Regenerates ThemeDefinitions.h from resources/themes/*.yml
generate-themes:
	python3 {{ justfile_directory() }}/bin/theme/generate.py \
		{{ justfile_directory() }}/src/ui/ThemeDefinitions.h \
		{{ justfile_directory() }}/resources/themes

# Reports theme contrast ratios (all themes by default, or pass one/more slugs)
theme-check-contrast *themes:
	python3 {{ justfile_directory() }}/bin/theme/contrast.py {{ themes }}

# Same as theme-check-contrast, but exits non-zero on hard AA failures
theme-check-contrast-strict *themes:
	python3 {{ justfile_directory() }}/bin/theme/contrast.py --fail-aa {{ themes }}

# No build step is required; the preview reads theme YAML files directly in the browser.
theme-preview-build:
	@echo "No build step required. Run 'just theme-preview-run'."

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

# Adds one locale-specific emoji search token override entry
emoji-add-token emoji locale token:
	python3 {{ justfile_directory() }}/bin/emoji/pipeline.py add-token "{{ emoji }}" "{{ locale }}" "{{ token }}" --repo-root {{ justfile_directory() }}

# Backward-compatible alias (deprecated; prefer emoji-build)
emoji-generate:
	just --justfile {{ justfile() }} emoji-build

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
icons-sync *args:
	{{ justfile_directory() }}/bin/icons/sync-fluent.sh {{ args }}

# Fetches one Fluent icon  (e.g. rel_path = assets/Something/something.svg) into resources/icons/fluent/ and wires a qrc alias
icons-fetch rel_path alias_svg_name:
	{{ justfile_directory() }}/bin/icons/fetch.sh "{{ rel_path }}" "{{ alias_svg_name }}"

# Removes the build directory
clean:
	rm -rf {{ build_dir }}

# Configures a debug build
configure-debug *args:
	cmake -S {{ justfile_directory() }} -B {{ build_dir }} \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
		-DMAN=OFF \
		{{ args }}

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
		{{ justfile_directory() }}/src/ \
		{{ justfile_directory() }}/resources/qml/ \
		-ts "${ts_files[@]}" \
		-no-obsolete
	just --justfile {{ justfile() }} translations-normalize

# Normalizes .ts files to a canonical XML format (idempotent)
translations-normalize *args:
	python3 {{ justfile_directory() }}/bin/translations/translate.py normalize {{ args }}

# Auto-translates unfinished strings for a language using Claude CLI
translations-claude-translate-lang lang *args:
	python3 {{ justfile_directory() }}/bin/translations/translate.py translate {{ lang }} {{ args }}

# Auto-translates unfinished strings for all languages using Claude CLI
translations-claude-translate-all *args: _ensure_just_temp_directory
	#!/usr/bin/env bash
	set -euo pipefail
	for d in {{ justfile_directory() }}/resources/langs/*/; do
		lang=$(basename "$d")
		if [[ "$lang" == "en" ]]; then
			continue
		fi
		if [[ ! -f "$d/komai_${lang}.ts" ]]; then
			continue
		fi
		echo "=== Translating: $lang ==="
		just --justfile {{ justfile() }} translations-claude-translate-lang "$lang" {{ args }} || {
			echo "ERROR: Translation failed for $lang, continuing..."
		}
	done

# Regenerates docs/architecture/settings/3-layer-mapping.md from settings source-of-truth files
settings-3-layer-mapping-generate *args:
	{{ justfile_directory() }}/bin/settings/settings-3-layer-mapping.sh {{ args }}

# Checks whether docs/architecture/settings/3-layer-mapping.md is up to date (no rewrite)
settings-3-layer-mapping-check *args:
	{{ justfile_directory() }}/bin/settings/settings-3-layer-mapping.sh check {{ args }}

# Checks Markdown links for local path validity
docs-check-links:
	python3 {{ justfile_directory() }}/bin/docs/check-links.py

# Summarizes room-switch performance markers from a log file
perf-room-switch-report logfile:
	python3 {{ justfile_directory() }}/bin/perf/room_switch_report.py "{{ logfile }}"

# Summarizes room-switch performance markers for a profile (default: default)
perf-room-switch-report-profile profile="default":
	python3 {{ justfile_directory() }}/bin/perf/room_switch_report.py --profile "{{ profile }}"

# Backward-compatible aliases (deprecated; prefer settings-3-layer-mapping-*)
settings-generate-3-layer-mapping *args:
	just --justfile {{ justfile() }} settings-3-layer-mapping-generate {{ args }}

settings-check-3-layer-mapping *args:
	just --justfile {{ justfile() }} settings-3-layer-mapping-check {{ args }}

# Regenerates etc/packaging/flatpak/cargo-sources.json from src/rust/Cargo.lock
flatpak-cargo-sources:
	python3 {{ justfile_directory() }}/bin/flatpak/cargo-sources.py {{ justfile_directory() }}

# Builds a Flatpak bundle from the local source tree
flatpak-build: _ensure_just_temp_directory emoji-fetch flatpak-cargo-sources
	#!/usr/bin/env bash
	set -euo pipefail

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
	flatpak build-bundle \
		"{{ flatpak_build_dir }}/repo" \
		"{{ flatpak_build_dir }}/komai.flatpak" \
		cc.etke.komai
	echo "Flatpak bundle: {{ flatpak_build_dir }}/komai.flatpak"

# Installs the locally-built Flatpak bundle
flatpak-install:
	flatpak --user install --or-update -y "{{ flatpak_build_dir }}/komai.flatpak"

# Runs the Flatpak-installed Komai
flatpak-run *args:
	flatpak run cc.etke.komai {{ args }}

# Removes the Flatpak build directory
flatpak-clean:
	rm -rf "{{ flatpak_build_dir }}"

# Builds an AppImage bundle inside a Docker container (works on any distro)
appimage-build-docker: emoji-fetch
	{{ justfile_directory() }}/etc/packaging/appimage/bin/build-docker "{{ justfile_directory() }}" "{{ appimage_build_dir }}"

# Builds an AppImage bundle natively (requires Ubuntu 25.04+ with appimage-builder installed)
appimage-build-native: emoji-fetch
	{{ justfile_directory() }}/etc/packaging/appimage/bin/build-native "{{ justfile_directory() }}" "{{ appimage_build_dir }}"

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
		check-theme-yaml \
		builtin-theme-wcag-aa \
		check-ts-normalized \
		qmllint \
		emoji-check \
		icons-audit \
		icons-list-check \
			icons-derived-check \
			settings-3-layer-mapping-check \
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
	hook="{{ justfile_directory() }}/.git/hooks/pre-commit"
	# The installed git hook runs later under Git, outside this just/mise environment.
	# Injecting PREK_HOME keeps prek's cache under var/prek instead of a global home dir,
	# which is more predictable and works better in sandboxed tools like Codex/OpenCode.
	if [ -f "$hook" ] && ! grep -q '^export PREK_HOME=' "$hook"; then
		sed -i '2iexport PREK_HOME="{{ prek_home }}"' "$hook"
	fi

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
