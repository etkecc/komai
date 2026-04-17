#!/usr/bin/env bash

set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/../.." && pwd)"
build_dir="${repo_root}/var/build/native"
lock_dir="${repo_root}/var/lock"
lock_file="${lock_dir}/native-build.lock"
info_file="${lock_dir}/native-build.lock.info"
jobs="${KOMAI_BUILD_JOBS:-$(nproc)}"
lock_fd=""
heartbeat_pid=""
lock_started_at="$(date --iso-8601=seconds)"
lock_state_dir=""
lock_heartbeat_file=""
lock_owner_pid="${BASHPID}"
cmake_target_help_cache=""

usage() {
	cat >&2 <<'EOF'
Usage:
  bin/build/native.sh configure [cmake args...]
  bin/build/native.sh configure-debug [cmake args...]
  bin/build/native.sh build [cmake --build args...]
  bin/build/native.sh rebuild [cmake args...]
  bin/build/native.sh test-cpp-unit [ctest args...]
  bin/build/native.sh test-cpp-integration [ctest args...]
  bin/build/native.sh test-cpp [ctest args...]
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

write_lock_info() {
	local heartbeat_at="${1:-}"

	{
		printf 'pid=%s\n' "${lock_owner_pid}"
		if [[ -n "${heartbeat_pid}" ]]; then
			printf 'heartbeat_pid=%s\n' "${heartbeat_pid}"
		fi
		printf 'started_at=%s\n' "${lock_started_at}"
		printf 'command='
		printf '%q ' "${lock_command[@]}"
		printf '\n'
		if [[ -n "${heartbeat_at}" ]]; then
			printf 'last_heartbeat_at=%s\n' "${heartbeat_at}"
		fi
	} >"${info_file}"
}

start_lock_heartbeat() {
	local interval="${1:-${KOMAI_BUILD_HEARTBEAT_SECONDS:-20}}"
	if ! [[ "${interval}" =~ ^[0-9]+$ ]] || [[ "${interval}" -le 0 ]]; then
		return
	fi

	(
		if [[ -n "${lock_fd}" ]]; then
			eval "exec ${lock_fd}>&-"
		fi

		while :; do
			sleep "${interval}" || exit 0

			if [[ "${PPID}" -ne "${lock_owner_pid}" ]]; then
				exit 0
			fi

			if ! kill -0 "${lock_owner_pid}" 2>/dev/null; then
				exit 0
			fi

			local heartbeat_at=""
			heartbeat_at="$(date --iso-8601=seconds)"

			if [[ -n "${lock_heartbeat_file}" ]]; then
				touch "${lock_heartbeat_file}" 2>/dev/null || exit 0
			fi

			write_lock_info "${heartbeat_at}" 2>/dev/null || exit 0
			echo "[native-build] still running: command=${command_name} pid=${lock_owner_pid} heartbeat_pid=${BASHPID} started_at=${lock_started_at} heartbeat_at=${heartbeat_at}" >&2
		done
	) &
	heartbeat_pid="$!"
	write_lock_info
}

cleanup_lock() {
	local status="${1:-0}"
	trap - EXIT INT TERM HUP

	if [[ -n "${heartbeat_pid}" ]]; then
		kill "${heartbeat_pid}" 2>/dev/null || true
		wait "${heartbeat_pid}" 2>/dev/null || true
	fi

	if [[ -n "${lock_state_dir}" ]]; then
		rm -rf "${lock_state_dir}"
	fi

	rm -f "${info_file}"

	if [[ -n "${lock_fd}" ]]; then
		eval "exec ${lock_fd}>&-"
	fi

	exit "${status}"
}

print_recorded_lock_holder() {
	if [[ ! -f "${info_file}" ]]; then
		return
	fi

	echo "Recorded lock holder:" >&2
	sed 's/^/  /' "${info_file}" >&2

	local recorded_pid=""
	recorded_pid="$(sed -n 's/^pid=//p' "${info_file}" | head -n1)"
	if [[ -z "${recorded_pid}" ]]; then
		return
	fi

	if ! kill -0 "${recorded_pid}" 2>/dev/null; then
		echo "  note=recorded lock metadata may be stale (pid ${recorded_pid} is not running)" >&2
		return
	fi

	local recorded_args=""
	recorded_args="$(ps -p "${recorded_pid}" -o args= 2>/dev/null || true)"
	if [[ "${recorded_args}" != *"bin/build/native.sh"* ]]; then
		echo "  note=recorded lock metadata may be stale (pid ${recorded_pid} is not a native.sh wrapper)" >&2
	fi
}

acquire_lock() {
	mkdir -p "${lock_dir}"

	exec {lock_fd}>"${lock_file}"

	if command -v flock >/dev/null 2>&1; then
		if ! flock -n "${lock_fd}"; then
			echo "Another native build/configure/test command is already running." >&2
			print_recorded_lock_holder
			echo "Refusing to run concurrently against ${build_dir}." >&2
			exit 73
		fi
	elif command -v python3 >/dev/null 2>&1; then
		if ! python3 -c 'import fcntl, sys; fcntl.flock(int(sys.argv[1]), fcntl.LOCK_EX | fcntl.LOCK_NB)' "${lock_fd}"; then
			echo "Another native build/configure/test command is already running." >&2
			print_recorded_lock_holder
			echo "Refusing to run concurrently against ${build_dir}." >&2
			exit 73
		fi
	else
		echo "Unable to acquire native build lock: neither 'flock' nor 'python3' is available." >&2
		exit 73
	fi

	write_lock_info
	start_lock_heartbeat
	trap 'cleanup_lock $?' EXIT INT TERM HUP
}

acquire_legacy_lock() {
	lock_state_dir="${lock_dir}/native-build.lock.d"
	lock_heartbeat_file="${lock_state_dir}/heartbeat"
	local lock_heartbeat_interval="${KOMAI_BUILD_LOCK_HEARTBEAT_SECONDS:-5}"
	local lock_stale_after="${KOMAI_BUILD_LOCK_STALE_SECONDS:-45}"

	if [[ -d "${lock_state_dir}" ]]; then
		local heartbeat_age=""
		local lock_owner_pid=""
		local lock_owner_dead="false"

		if [[ -f "${info_file}" ]]; then
			lock_owner_pid="$(sed -n 's/^pid=//p' "${info_file}" | head -n1)"
			if [[ -n "${lock_owner_pid}" ]]; then
				if ! kill -0 "${lock_owner_pid}" 2>/dev/null; then
					lock_owner_dead="true"
				else
					local lock_owner_args=""
					lock_owner_args="$(ps -p "${lock_owner_pid}" -o args= 2>/dev/null || true)"
					if [[ "${lock_owner_args}" != *"bin/build/native.sh"* ]]; then
						lock_owner_dead="true"
					fi
				fi
			fi
		fi

		if [[ -f "${lock_heartbeat_file}" ]]; then
			local now
			now="$(date +%s)"
			local beat
			beat="$(stat -c %Y "${lock_heartbeat_file}")"
			heartbeat_age="$((now - beat))"
		fi

		if [[ "${lock_owner_dead}" == "true" ]]; then
			echo "Removing stale native build lock (owner pid ${lock_owner_pid} is no longer running)." >&2
			rm -rf "${lock_state_dir}"
		elif [[ -n "${heartbeat_age}" && "${heartbeat_age}" -gt "${lock_stale_after}" ]]; then
			echo "Removing stale native build lock (last heartbeat ${heartbeat_age}s ago)." >&2
			rm -rf "${lock_state_dir}"
		fi
	fi

	if ! mkdir "${lock_state_dir}" 2>/dev/null; then
		echo "Another native build/configure/test command is already running." >&2
		print_recorded_lock_holder
		if [[ -f "${lock_heartbeat_file}" ]]; then
			local now
			now="$(date +%s)"
			local beat
			beat="$(stat -c %Y "${lock_heartbeat_file}")"
			echo "  heartbeat_age_seconds=$((now - beat))" >&2
		fi
		echo "Refusing to run concurrently against ${build_dir}." >&2
		exit 73
	fi

	write_lock_info
	touch "${lock_heartbeat_file}"
	start_lock_heartbeat "${lock_heartbeat_interval}"
	trap 'cleanup_lock $?' EXIT INT TERM HUP
}

# Ensure the pinned Rust toolchain is installed.  Corrosion's FindRust checks
# `rustup toolchain list` but does NOT auto-install missing toolchains, so a
# fresh clone would fail without this.
ensure_rust_toolchain() {
	local toolchain_file="${repo_root}/rust-toolchain.toml"
	if [[ ! -f "${toolchain_file}" ]]; then
		echo "WARNING: ${toolchain_file} not found — cannot verify Rust toolchain." >&2
		return
	fi
	local channel
	channel="$(sed -n 's/^channel[[:space:]]*=[[:space:]]*"\(.*\)"/\1/p' "${toolchain_file}")"
	if [[ -z "${channel}" ]]; then
		echo "WARNING: could not parse channel from ${toolchain_file} — cannot verify Rust toolchain." >&2
		return
	fi
	if ! command -v rustup >/dev/null 2>&1; then
		echo "WARNING: rustup not found — cannot auto-install Rust toolchain ${channel}." >&2
		echo "  Install rustup (https://rustup.rs/) or manually install Rust ${channel}." >&2
		return
	fi
	if rustup toolchain list | grep -q "^${channel}"; then
		return
	fi
	echo "Installing Rust toolchain ${channel} (from rust-toolchain.toml)..."
	rustup toolchain install "${channel}"
}

needs_release_configure() {
	if [[ ! -f "${build_dir}/CMakeCache.txt" ]]; then
		return 0
	fi

	if ! grep -q '^CMAKE_BUILD_TYPE:STRING=Release$' "${build_dir}/CMakeCache.txt"; then
		return 0
	fi

	if ! grep -q '^MAN:BOOL=OFF$' "${build_dir}/CMakeCache.txt"; then
		return 0
	fi

	# Verify the cached Rust toolchain matches rust-toolchain.toml.
	# Corrosion caches resolved rustc/cargo paths as INTERNAL CMake variables,
	# so a stale cache silently uses the wrong compiler even after reconfigure.
	# The only reliable fix is wiping the build directory.
	local expected_channel
	expected_channel="$(sed -n 's/^channel[[:space:]]*=[[:space:]]*"\(.*\)"/\1/p' "${repo_root}/rust-toolchain.toml")"
	if [[ -n "${expected_channel}" ]]; then
		local cached_toolchain
		cached_toolchain="$(sed -n 's/^Rust_TOOLCHAIN:STRING=//p' "${build_dir}/CMakeCache.txt")"
		if [[ -n "${cached_toolchain}" && "${cached_toolchain}" != "${expected_channel}-"* ]]; then
			echo "Cached Rust toolchain (${cached_toolchain}) does not match rust-toolchain.toml (${expected_channel})." >&2
			echo "Clearing build directory for reconfiguration..." >&2
			rm -rf "${build_dir}"
			return 0
		fi
	fi

	return 1
}

configure_release() {
	cmake -S "${repo_root}" -B "${build_dir}" \
		"$@"
}

configure_debug() {
	cmake -S "${repo_root}" -B "${build_dir}" \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
		"$@"
}

build_target() {
	local target="$1"
	local parallelism="${2:-${jobs}}"
	if [[ $# -ge 2 ]]; then
		shift 2 || true
	else
		shift || true
	fi

	cmake --build "${build_dir}" --parallel "${parallelism}" --target "${target}" "$@"
}

cmake_target_exists() {
	local target="$1"

	if [[ -z "${cmake_target_help_cache}" ]]; then
		cmake_target_help_cache="$(cmake --build "${build_dir}" --target help 2>/dev/null || true)"
	fi

	grep -Fq "... ${target}" <<<"${cmake_target_help_cache}"
}

build_runtime_bundle() {
	local extra_args=("$@")

	build_target komai "${jobs}" "${extra_args[@]}"

	if cmake_target_exists komai-mcp; then
		build_target komai-mcp 1 "${extra_args[@]}"
	elif cmake_target_exists cargo-build_komai-mcp; then
		build_target cargo-build_komai-mcp 1 "${extra_args[@]}"
	fi
}

run_test_label() {
	local label="$1"
	shift || true

	build_target komai_tests
	ctest --test-dir "${build_dir}" --output-on-failure -L "${label}" "$@"
}

if command -v flock >/dev/null 2>&1 || command -v python3 >/dev/null 2>&1; then
	acquire_lock
else
	acquire_legacy_lock
fi

case "${command_name}" in
configure)
	ensure_rust_toolchain
	configure_release "$@"
	;;
configure-debug)
	ensure_rust_toolchain
	configure_debug "$@"
	;;
build)
	ensure_rust_toolchain
	if needs_release_configure; then
		configure_release
	fi
	build_runtime_bundle "$@"
	;;
rebuild)
	ensure_rust_toolchain
	rm -rf "${build_dir}"
	configure_release "$@"
	build_runtime_bundle
	;;
test-cpp-unit|test-unit)
	ensure_rust_toolchain
	if needs_release_configure; then
		configure_release
	fi
	run_test_label unit "$@"
	;;
test-cpp-integration|test-integration)
	ensure_rust_toolchain
	if needs_release_configure; then
		configure_release
	fi
	run_test_label integration "$@"
	;;
test-cpp|test-all)
	ensure_rust_toolchain
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
	find "${repo_root}/src/rust" -maxdepth 2 -type d -name target -exec rm -rf {} + 2>/dev/null || true
	;;
*)
	usage
	;;
esac
