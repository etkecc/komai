# Shows help
default:
	@just --list --justfile {{ justfile() }}

build_dir := justfile_directory() / "build"

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

# Runs the linter/formatter
lint:
	{{ justfile_directory() }}/.ci/format.sh
