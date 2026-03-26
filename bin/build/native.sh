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

	{
		printf 'pid=%s\n' "$$"
		printf 'started_at=%s\n' "$(date --iso-8601=seconds)"
		printf 'command='
		printf '%q ' "${lock_command[@]}"
		printf '\n'
	} >"${info_file}"

	trap 'status=$?; rm -f "${info_file}"; if [[ -n "${lock_fd}" ]]; then eval "exec ${lock_fd}>&-"; fi; exit "${status}"' EXIT INT TERM HUP
}

acquire_legacy_lock() {
	local lock_state_dir="${lock_dir}/native-build.lock.d"
	local heartbeat_file="${lock_state_dir}/heartbeat"
	local heartbeat_pid=""
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

		if [[ -f "${heartbeat_file}" ]]; then
			local now
			now="$(date +%s)"
			local beat
			beat="$(stat -c %Y "${heartbeat_file}")"
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
		if [[ -f "${heartbeat_file}" ]]; then
			local now
			now="$(date +%s)"
			local beat
			beat="$(stat -c %Y "${heartbeat_file}")"
			echo "  heartbeat_age_seconds=$((now - beat))" >&2
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
	touch "${heartbeat_file}"

	(
		while :; do
			touch "${heartbeat_file}" 2>/dev/null || exit 0
			sleep "${lock_heartbeat_interval}" || exit 0
		done
	) &
	heartbeat_pid="$!"

	trap "status=\$?; if [[ -n \"${heartbeat_pid}\" ]]; then kill \"${heartbeat_pid}\" 2>/dev/null || true; wait \"${heartbeat_pid}\" 2>/dev/null || true; fi; rm -rf \"${lock_state_dir}\"; rm -f \"${info_file}\"; exit \"\${status}\"" EXIT INT TERM HUP
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

build_runtime_bundle() {
	local extra_args=("$@")

	build_target komai "${jobs}" "${extra_args[@]}"
	build_target komai-mcp 1 "${extra_args[@]}"
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
	configure_release "$@"
	;;
configure-debug)
	configure_debug "$@"
	;;
build)
	if needs_release_configure; then
		configure_release
	fi
	build_runtime_bundle "$@"
	;;
rebuild)
	rm -rf "${build_dir}"
	configure_release "$@"
	build_runtime_bundle
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
