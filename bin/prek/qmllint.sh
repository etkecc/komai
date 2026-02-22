#!/usr/bin/env sh

set -eu

warn_if_present() {
    if printf '%s\n' "$1" | grep -qE "([Ww]arning|^[[:space:]]*warning:)"; then
        exit 1
    fi
}

if command -v qmake6 >/dev/null 2>&1; then
    q="$(qmake6 -query QT_INSTALL_BINS)/qmllint"
    if [ -x "$q" ]; then
        set +e
        output=$("$q" "$@" 2>&1)
        status=$?
        set -e

        printf '%s\n' "$output"
        if [ "$status" -ne 0 ]; then
            exit "$status"
        fi
        warn_if_present "$output"
        exit 0
    fi
fi

if command -v qmllint >/dev/null 2>&1; then
    q="$(command -v qmllint)"
    if ldd "$q" 2>/dev/null | grep -q "Qt6"; then
        set +e
        output=$("$q" "$@" 2>&1)
        status=$?
        set -e

        printf '%s\n' "$output"
        if [ "$status" -ne 0 ]; then
            exit "$status"
        fi
        warn_if_present "$output"
        exit 0
    fi
fi

echo "qmllint (Qt6) unavailable; skipping."
exit 0
