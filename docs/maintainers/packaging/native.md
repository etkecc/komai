# Native build

Build and install Komai directly on your system using CMake.

## Prerequisites

- [just](https://github.com/casey/just) command runner (optional but recommended)
- **Python 3** (for theme generation and emoji data generation at build time)
- **CMake** 3.15+
- **C++20 compiler**: GCC 11.3+, Clang 16+, or MSVC 19.13+
- **Rust** 1.70+ (rustc + cargo) -- for the CXX-bridged Rust crate (`resolvematrix`)

## Quick start

```sh
git clone https://github.com/etkecc/komai && cd komai
just build
just run
```

The build generates derived logo assets from `resources/komai.svg` into
`var/build/native/logo-assets/`. This intentionally uses a small Qt helper
target instead of external tools like ImageMagick or `rsvg-convert`, so normal
Linux builds, distro packaging, and CI do not need extra raster conversion
dependencies beyond the existing Qt build stack.

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
| [Python 3](https://www.python.org/) | | Theme generation and emoji data generation at build time |
| [cmark](https://github.com/commonmark/cmark) | 0.29 | Markdown rendering |
| [KSyntaxHighlighting](https://api.kde.org/frameworks/syntax-highlighting/html/index.html) | 6.x | Timeline formatted-message code block syntax highlighting |
| [spdlog](https://github.com/gabime/spdlog) | | Logging |
| [fmt](https://github.com/fmtlib/fmt) | | String formatting |
| [yaml-cpp](https://github.com/jbeder/yaml-cpp) | 0.6 | Settings storage |
| [OpenSSL](https://www.openssl.org/) | 1.1.0 | TLS/cryptography |
| [nlohmann-json](https://github.com/nlohmann/json) | 3.2.0 | JSON parsing |
| [qtkeychain](https://github.com/frankosterfeld/qtkeychain) | 0.12 | Credential storage |
| [KDSingleApplication](https://github.com/KDAB/KDSingleApplication) | 1.0 | Single-instance support |

### Optional

| Dependency | Purpose | CMake flag to disable |
|-----------|---------|----------------------|
| [GStreamer](https://gitlab.freedesktop.org/gstreamer) 1.20+ | VoIP (voice & video calls) | `-DVOIP=OFF` |
| XCB, XCB-EWMH | X11 screensharing | `-DSCREENSHARE_X11=OFF` |

### System packages vs CPM downloads

By default, most dependencies are downloaded and built by
[CPM.cmake](https://github.com/cpm-cmake/CPM.cmake).
Distro packagers can use system packages instead:

```sh
# Use system packages for all dependencies
just configure -DCPM_USE_LOCAL_PACKAGES=ON

# Use a system package for a single library
just configure -DCPM_Qt6Keychain_USE_LOCAL=ON
```

See [CPM.cmake options](https://github.com/cpm-cmake/CPM.cmake#options) for details.

## Distro-specific package lists

### Arch Linux

```sh
sudo pacman -S --needed --asdeps qt6-base qt6-declarative qt6-tools qt6-multimedia qt6-svg \
    cmake gcc fontconfig python rust \
    cmark syntax-highlighting spdlog fmt openssl \
    nlohmann-json yaml-cpp qtkeychain-qt6 kdsingleapplication litehtml
```

### Debian 13+ / Ubuntu 24.04+

```sh
sudo apt install -y build-essential cmake pkg-config python3 cargo rustc \
    libspdlog-dev libfmt-dev \
    libcurl4-openssl-dev libssl-dev libcmark-dev \
    libkf6syntaxhighlighting-dev \
    nlohmann-json3-dev libyaml-cpp-dev libkdsingleapplication-qt6-dev \
    qt6-base-dev qt6-tools-dev qt6-svg-dev qt6-multimedia-dev \
    qt6-declarative-dev qtkeychain-qt6-dev qt6-base-private-dev \
    qt6-declarative-private-dev
```

By default, CPM downloads and builds all non-system C++ dependencies
(`litehtml`, `blurhash`, `cpp-httplib`, etc.), while Cargo resolves
the Rust runtime crates (`matrix-sdk`, `matrix-sdk-ui`, and friends). Pass
`-DCPM_USE_LOCAL_PACKAGES=ON` to prefer system packages instead.

Note: macOS bundle builds also need the Qt installation used for the build to
include ICNS imageformat support, because the app bundle icon is generated from
`resources/komai.svg` during the build.

## Debug builds

```sh
just configure-debug
just build
```

This enables debug symbols and generates `compile_commands.json` (for LSP/IDE integration). The output is in the same `var/build/native/` directory.
