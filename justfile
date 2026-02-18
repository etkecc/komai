# Paths
build_dir := justfile_directory() / "build"
flatpak_build_dir := justfile_directory() / "build-flatpak"

# mise (dev tool version manager)
mise_data_dir := env("MISE_DATA_DIR", justfile_directory() / "var/mise")
mise_trusted_config_paths := justfile_directory() / "mise.toml"

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
build *args:
	#!/usr/bin/env bash
	set -euo pipefail
	if [[ ! -f "{{ build_dir }}/CMakeCache.txt" ]]; then
		just --justfile {{ justfile() }} configure
	fi
	cmake --build {{ build_dir }} --parallel "$(nproc)" {{ args }}

# Configures and builds from scratch
rebuild *args:
	just --justfile {{ justfile() }} configure {{ args }}
	just --justfile {{ justfile() }} build

# Installs the compiled binary (may require sudo)
install:
	cmake --install {{ build_dir }}

# Runs the compiled binary (builds first if needed)
run *args:
	#!/usr/bin/env bash
	set -euo pipefail
	binary="{{ build_dir }}/komai"
	if [[ ! -x "$binary" ]]; then
		just --justfile {{ justfile() }} build
	fi
	exec "$binary" {{ args }}

# Regenerates ThemeDefinitions.h from resources/themes/*.yaml
generate-themes:
	python3 {{ justfile_directory() }}/bin/generate-themes.py \
		{{ justfile_directory() }}/src/ui/ThemeDefinitions.h \
		{{ justfile_directory() }}/resources/themes

# Imports a Base16 theme from tinted-theming/schemes. Use --list to see available themes.
import-theme *args:
	python3 {{ justfile_directory() }}/bin/import-theme.py {{ args }}

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
translations-update:
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
	python3 {{ justfile_directory() }}/bin/translations-translate.py normalize {{ args }}

# Auto-translates unfinished strings for a language using Claude CLI
translations-claude-translate-lang lang *args:
	python3 {{ justfile_directory() }}/bin/translations-translate.py translate {{ lang }} {{ args }}

# Auto-translates unfinished strings for all languages using Claude CLI
translations-claude-translate-all *args:
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

# Builds a Flatpak bundle from the local source tree
flatpak-build:
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

# Runs the linter/formatter
lint:
	{{ justfile_directory() }}/.ci/format.sh

# Invokes mise with the project-local data directory
mise *args: _ensure_mise_data_directory
	#!/bin/sh
	export MISE_DATA_DIR="{{ mise_data_dir }}"
	export MISE_TRUSTED_CONFIG_PATHS="{{ mise_trusted_config_paths }}"
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
	@just --justfile {{ justfile() }} mise exec -- prek install

# Internal - ensures var/mise directory exists
_ensure_mise_data_directory:
	#!/bin/sh
	if [ ! -d "{{ mise_data_dir }}" ]; then
		mkdir -p "{{ mise_data_dir }}"
	fi

# Internal - ensures mise tools are installed
_ensure_mise_tools_installed: _ensure_mise_data_directory
	@just --justfile {{ justfile() }} mise install --quiet
