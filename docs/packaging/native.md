# Native build

Build and install Komai directly on your system using CMake.

## Prerequisites

- [just](https://github.com/casey/just) command runner (optional but recommended)
- **Python 3** (for theme generation at build time)
- **CMake** 3.15+
- **C++20 compiler**: GCC 11.3+, Clang 16+, or MSVC 19.13+

## Quick start

```sh
git clone https://github.com/etkecc/komai && cd komai
just build
just run
```

## Justfile recipes

| Recipe | What it does |
|--------|-------------|
| `just build` | Configure (if needed) + build |
| `just rebuild` | Clean configure + build |
| `just run` | Build if needed, then run |
| `just install` | Install system-wide (may require `sudo`) |
| `just clean` | Remove build directory (`var/build/native/`) |
| `just configure` | Run CMake configure (Release mode) |
| `just configure-debug` | Configure a debug build (with `compile_commands.json`) |

All recipes accept extra arguments, e.g. `just configure -DVOIP=OFF`.

## Without just

```sh
cmake -S. -Bvar/build/native -DCMAKE_BUILD_TYPE=Release -DMAN=OFF
cmake --build var/build/native --parallel $(nproc)

# Run
./var/build/native/komai

# Install system-wide
sudo cmake --install var/build/native
```

## Dependencies

### Required

| Dependency | Minimum version | Notes |
|-----------|----------------|-------|
| [Qt6](https://www.qt.io/) | 6.5 | Base, Declarative, Multimedia, SVG, Tools |
| [CMake](https://cmake.org/) | 3.15 | |
| [Python 3](https://www.python.org/) | | Theme generation at build time |
| [mtxclient](https://github.com/Nheko-Reborn/mtxclient) | | Matrix client library |
| [coeurl](https://nheko.im/Nheko-Reborn/coeurl) | | HTTP library |
| [LMDB](https://www.symas.com/lmdb) | | Database |
| [lmdb++](https://github.com/hoytech/lmdbxx) | | C++ LMDB wrapper |
| [cmark](https://github.com/commonmark/cmark) | 0.29 | Markdown rendering |
| [libolm](https://gitlab.matrix.org/matrix-org/olm) | | E2EE |
| [spdlog](https://github.com/gabime/spdlog) | | Logging |
| [fmt](https://github.com/fmtlib/fmt) | | String formatting |
| [yaml-cpp](https://github.com/jbeder/yaml-cpp) | 0.6 | Settings storage |
| [re2](https://github.com/google/re2) | | Regular expressions |
| [OpenSSL](https://www.openssl.org/) | 1.1.0 | TLS/cryptography |
| [nlohmann-json](https://github.com/nlohmann/json) | 3.2.0 | JSON parsing |
| [qtkeychain](https://github.com/frankosterfeld/qtkeychain) | 0.12 | Credential storage |
| [KDSingleApplication](https://github.com/KDAB/KDSingleApplication) | 1.0 | Single-instance support |

### Optional

| Dependency | Purpose | CMake flag to disable |
|-----------|---------|----------------------|
| [GStreamer](https://gitlab.freedesktop.org/gstreamer) 1.20+ | VoIP (voice & video calls) | `-DVOIP=OFF` |
| XCB, XCB-EWMH | X11 screensharing | `-DSCREENSHARE_X11=OFF` |

### Bundling dependencies

Most dependencies can be bundled automatically instead of using system packages:

```sh
# Bundle everything
just configure -DHUNTER_ENABLED=ON -DBUILD_SHARED_LIBS=OFF

# Or bundle specific libraries
just configure -DUSE_BUNDLED_COEURL=ON -DUSE_BUNDLED_MTXCLIENT=ON -DUSE_BUNDLED_LMDBXX=ON
```

## Distro-specific package lists

### Arch Linux

```sh
sudo pacman -S qt6-base qt6-declarative qt6-tools qt6-multimedia qt6-svg \
    cmake gcc fontconfig \
    mtxclient coeurl libolm lmdb lmdbxx cmark spdlog fmt re2 openssl \
    nlohmann-json yaml-cpp qtkeychain-qt6 kdsingleapplication
```

### Debian 13+ / Ubuntu 24.04+

```sh
sudo apt install -y build-essential cmake pkg-config python3 \
    libevent-dev libspdlog-dev libfmt-dev libre2-dev \
    liblmdb++-dev libcurl4-openssl-dev libssl-dev libolm-dev libcmark-dev \
    nlohmann-json3-dev libyaml-cpp-dev libkdsingleapplication-qt6-dev \
    qt6-base-dev qt6-tools-dev qt6-svg-dev qt6-multimedia-dev \
    qt6-declarative-dev qtkeychain-qt6-dev qt6-base-private-dev \
    qt6-declarative-private-dev
```

Some dependencies may need bundling on Debian/Ubuntu:

```sh
just build -DUSE_BUNDLED_COEURL=1 -DUSE_BUNDLED_MTXCLIENT=1 -DUSE_BUNDLED_LMDBXX=1
```

## Debug builds

```sh
just configure-debug
just build
```

This enables debug symbols and generates `compile_commands.json` (for LSP/IDE integration). The output is in the same `var/build/native/` directory.
