#!/usr/bin/env sh

set -eu

if command -v qmake6 >/dev/null 2>&1; then
    q="$(qmake6 -query QT_INSTALL_BINS)/qmllint"
    if [ -x "$q" ]; then
        exec "$q" "$@"
    fi
fi

if command -v qmllint >/dev/null 2>&1; then
    q="$(command -v qmllint)"
    if ldd "$q" 2>/dev/null | grep -q "Qt6"; then
        exec "$q" "$@"
    fi
fi

echo "qmllint (Qt6) unavailable; skipping."
exit 0
