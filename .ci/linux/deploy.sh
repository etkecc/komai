#!/usr/bin/env sh

# Legacy AppImage deploy script -- now delegates to the build scripts.
# Kept for backward compatibility with CI scripts that reference it.
#
# For local builds, use: just appimage-build-docker

set -ex

etc/packaging/appimage/bin/build-native . var/build/appimage
mkdir -p artifacts
cp var/build/appimage/komai-latest-x86_64.AppImage artifacts/
