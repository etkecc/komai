# SPDX-FileCopyrightText: Komai Contributors
#
# SPDX-License-Identifier: GPL-3.0-or-later

# Base image used by .github/workflows/ci.yml. Pre-installs every pacman
# dependency CI needs so the workflow doesn't have to re-download them
# on every run, and runs as a non-root user whose UID matches the host
# self-hosted runner user — so bind-mounted workspace files are never
# left root-owned, which would block subsequent host-mode jobs (e.g.
# packaging-check.yml, publish.yml) from cleaning up.
#
# Rebuilt by .github/workflows/ci-image.yml on Dockerfile changes,
# weekly via cron (Arch rolls fast), and on manual dispatch.

FROM docker.io/archlinux:base-devel

# Keep Arch package resolution non-interactive by pinning concrete
# providers for virtual deps that otherwise trigger prompts:
# - qt6-multimedia-backend -> qt6-multimedia-ffmpeg
# - org.freedesktop.secrets -> kwallet
# - jack -> pipewire-jack (pulled transitively via ffmpeg)
#
# -Syu (not just -Sy) is required on Arch to avoid partial-update
# breakage between the base image's pacman db and current repos.
#
# Use rustup instead of Arch's rolling `rust` package so Corrosion and
# rust-toolchain.toml honor the repo's pinned toolchain.
#
# qt6-webengine + qt6-webchannel back the ELEMENT_CALL build option (Element
# Call's QtWebEngine webview), which defaults ON, so CMake's
# find_package(... WebEngineQuick WebChannelQuick REQUIRED) needs both here.
RUN pacman -Syu --noconfirm --needed \
        git \
        ccache mold \
        cmake just python python-yaml mise reuse rustup \
        qt6-base qt6-declarative qt6-tools qt6-multimedia qt6-multimedia-ffmpeg qt6-svg \
        qt6-webengine qt6-webchannel \
        cmark openssl \
        qtkeychain-qt6 kdsingleapplication \
        curl libsecret kwallet pipewire-jack \
        gst-plugins-bad-libs \
        github-cli \
    && pacman -Scc --noconfirm

# Arch base images ship without any locale generated, so processes
# inherit LANG=C / LC_CTYPE=ANSI_X3.4-1968. Qt detects that at every
# QGuiApplication startup and prints a multi-line warning before
# auto-switching to "C.UTF-8" itself, which fires once per test
# binary in `just test-cpp` and spams the CI log. Pin the env to
# C.UTF-8 (a glibc built-in, no locale-gen needed) so Qt sees a
# UTF-8 locale up front and stays quiet.
ENV LANG=C.UTF-8

# Build args passed by ci-image.yml — values derived at build time from
# `stat` on the bind-mounted workspace (whose owner is the host runner
# user). Defaults are a sane fallback for local builds.
ARG CI_UID=1000
ARG CI_GID=1000

RUN groupadd --gid "${CI_GID}" ci \
    && useradd --create-home --uid "${CI_UID}" --gid "${CI_GID}" --shell /bin/bash ci

USER ci
WORKDIR /home/ci
