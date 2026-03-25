#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
build_dir="${repo_root}/var/build/native"
lock_dir="${repo_root}/var/lock"
lock_file="${lock_dir}/native-build.lock"
info_file="${lock_dir}/native-build.lock.info"
jobs="${KOMAI_BUILD_JOBS:-$(nproc)}"

usage() {
	cat >&2 <<'EOF'
Usage:
  bin/build/native.sh configure [cmake args...]
  bin/build/native.sh configure-debug [cmake args...]
  bin/build/native.sh build [cmake --build args...]
  bin/build/native.sh rebuild [cmake args...]
  bin/build/native.sh test-unit [ctest args...]
  bin/build/native.sh test-integration [ctest args...]
  bin/build/native.sh test-all [ctest args...]
  bin/build/native.sh install [cmake --install args...]
  bin/build/native.sh clean
EOF
	exit 2
}

command_name="${1:-}"
if [[ -z "${command_name}" ]]; then
	usage
fi
shift || true

lock_command=("${command_name}" "$@")

acquire_lock() {
	mkdir -p "${lock_dir}"
	exec 9>"${lock_file}"

	if ! flock -n 9; then
		echo "Another native build/configure/test command is already running." >&2
		if [[ -f "${info_file}" ]]; then
			echo "Current lock holder:" >&2
			sed 's/^/  /' "${info_file}" >&2
		fi
		echo "Refusing to run concurrently against ${build_dir}." >&2
		exit 73
	fi

	{
		printf 'pid=%s\n' "$$"
		printf 'started_at=%s\n' "$(date --iso-8601=seconds)"
		printf 'command='
		printf '%q ' "${lock_command[@]}"
		printf '\n'
	} >"${info_file}"

	trap 'rm -f "${info_file}"' EXIT
}

needs_release_configure() {
	if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
		return 0
	fi

	grep -q '^USE_BUNDLED_MTXCLIENT:BOOL=OFF$' "${build_dir}/CMakeCache.txt"
}

configure_release() {
	cmake -S "${repo_root}" -B "${build_dir}" \
		-DCMAKE_BUILD_TYPE=Release \
		-DMAN=OFF \
		"$@"
}

configure_debug() {
	cmake -S "${repo_root}" -B "${build_dir}" \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
		-DMAN=OFF \
		"$@"
}

stage_rust_targets() {
	cmake --build "${build_dir}" --parallel 1 \
		--target cargo-build_komai_rust cargo-build_komai-mcp
}

build_target() {
	local target="$1"
	shift || true

	stage_rust_targets
	cmake --build "${build_dir}" --parallel "${jobs}" --target "${target}" "$@"
}

run_test_label() {
	local label="$1"
	shift || true

	build_target komai_tests
	ctest --test-dir "${build_dir}" --output-on-failure -L "${label}" "$@"
}

acquire_lock

case "${command_name}" in
configure)
	configure_release "$@"
	;;
configure-debug)
	configure_debug "$@"
	;;
build)
	if needs_release_configure; then
		configure_release
	fi
	build_target komai "$@"
	;;
rebuild)
	rm -rf "${build_dir}"
	configure_release "$@"
	build_target komai
	;;
test-unit)
	if needs_release_configure; then
		configure_release
	fi
	run_test_label unit "$@"
	;;
test-integration)
	if needs_release_configure; then
		configure_release
	fi
	run_test_label integration "$@"
	;;
test-all)
	if needs_release_configure; then
		configure_release
	fi
	run_test_label unit "$@"
	ctest --test-dir "${build_dir}" --output-on-failure -L integration "$@"
	;;
install)
	cmake --install "${build_dir}" "$@"
	;;
clean)
	rm -rf "${build_dir}"
	;;
*)
	usage
	;;
esac
