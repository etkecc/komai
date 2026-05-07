# Native build

Build and install Komai directly on your system using CMake.

## Prerequisites

- [just](https://github.com/casey/just) command runner (optional but recommended)
- **Python 3** (for theme generation and emoji data generation at build time)
- **CMake** 3.15+
- **C++20 compiler**: GCC 11.3+, Clang 16+, or MSVC 19.13+
- **Rust** 1.95.0 toolchain (rustc + cargo; `rustup` recommended) -- for the matrix-sdk runtime,
  settings/theme/syntax helpers, and supporting Rust crates

Komai pins Rust via [`rust-toolchain.toml`](../../../rust-toolchain.toml), and CMake defaults
Corrosion to the `1.95.0` rustup toolchain.

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
| [OpenSSL](https://www.openssl.org/) | 1.1.0 | TLS/cryptography |
| [qtkeychain](https://github.com/frankosterfeld/qtkeychain) | 0.12 | Credential storage |
| [KDSingleApplication](https://github.com/KDAB/KDSingleApplication) | 1.0 | Single-instance support |

### Optional

| Dependency | Purpose | CMake flag to disable |
|-----------|---------|----------------------|
| [GStreamer](https://gitlab.freedesktop.org/gstreamer) 1.20+ | VoIP (voice & video calls) | `-DVOIP=OFF` |
| XCB, XCB-EWMH | X11 screensharing and window roles | `-DX11=OFF` |

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
    openssl \
    qtkeychain-qt6 kdsingleapplication litehtml
```

For VoIP (voice/video calls; skip if you build with `-DVOIP=OFF`):

```sh
sudo pacman -S --needed --asdeps gst-plugins-base gst-plugins-good gst-plugins-bad \
    libnice gst-plugin-qml6 gst-plugin-pipewire
```

`gst-plugin-qml6` is the Qt6 GL video sink — without it, video calls
fail to render at runtime (`module "org.freedesktop.gstreamer.Qt6GLVideoItem"
is not installed`). `gst-plugins-bad` provides `webrtcbin`, which is
what gates calls in the first place. `gst-plugin-pipewire` provides
`pipewiresrc`, the GStreamer source element used to capture screen
frames from xdg-desktop-portal on Wayland; without it, screen-share
invitations fail with an install hint and the in-dialog Preview button
does the same.

### Debian 13+ / Ubuntu 24.04+

```sh
sudo apt install -y build-essential cmake pkg-config python3 cargo rustc \
    libssl-dev \
    libkdsingleapplication-qt6-dev \
    qt6-base-dev qt6-tools-dev qt6-svg-dev qt6-multimedia-dev \
    qt6-declarative-dev qtkeychain-qt6-dev qt6-base-private-dev \
    qt6-declarative-private-dev
```

For VoIP (voice/video calls; skip if you build with `-DVOIP=OFF`):

```sh
sudo apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev libnice-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-nice gstreamer1.0-qt6 \
    gstreamer1.0-pipewire
```

By default, CPM downloads and builds all non-system C++ dependencies
(`litehtml`, etc.), while Cargo resolves
the Rust runtime crates (`matrix-sdk`, `matrix-sdk-ui`, and friends). Pass
`-DCPM_USE_LOCAL_PACKAGES=ON` to prefer system packages instead.

If your distro `cargo`/`rustc` packages do not provide Rust `1.95.0`, install that toolchain via
`rustup toolchain install 1.95.0` so Corrosion can use the pinned version from
`rust-toolchain.toml`.

If you would rather build against whatever `rustc`/`cargo` your distro
ships, remove `rust-toolchain.toml` from the source tree before configuring;
Komai's CMake glue keys off the file's presence, and `rustup`'s shim (if you
have one) will then leave its toolchain selection alone. Komai is not coupled
to a specific toolchain version, so any reasonably recent stable Rust should
work. This is the route the Arch PKGBUILD takes.

Note: macOS bundle builds also need the Qt installation used for the build to
include ICNS imageformat support, because the app bundle icon is generated from
`resources/komai.svg` during the build.

## 🪟 Windows and macOS (untested)

> ⚠️ **Untested by maintainers.** Komai has never been built on Windows
> or macOS by the project. The notes below are tentative pointers based
> on the build system's cross-platform underpinnings (Qt 6, CMake, Rust),
> not verified instructions. If you get a build working — or hit issues
> we should document — contributions to flesh out this section are very
> welcome.

The Linux build is driven by CMake (with `just` as a thin convenience
wrapper) and CPM downloads most C++ dependencies. Both are
cross-platform, so a build *should* be theoretically achievable on
Windows and macOS provided the [Prerequisites](#prerequisites) above are
installed.

### Common prerequisites (in addition to the ones at the top of this page)

- A [Qt 6.5+](https://www.qt.io/download) installation including the
  Base, Declarative, Multimedia, SVG, and Tools modules. The official Qt
  installer is the simplest path on both platforms.
- [Python 3](https://www.python.org/downloads/) on `PATH`.
- [Rust 1.95.0](https://rustup.rs/) — `rustup` will pick the toolchain
  pinned in [`rust-toolchain.toml`](../../../rust-toolchain.toml)
  automatically.
- A C++20 compiler:
  - **Windows:** Visual Studio 2022 (MSVC 19.13+) or LLVM/clang-cl.
  - **macOS:** Xcode 14+ or a recent Apple Clang from the Command Line
    Tools.

### Windows-specific notes

- Run the build from a *Developer Command Prompt for VS 2022* (or the
  PowerShell variant) so MSVC, CMake, and Qt are all on `PATH` together.
- VOIP (voice & video calls) depends on GStreamer, which is Linux-leaning
  — easiest first attempt is to build with `-DVOIP=OFF`.
- X11 features (screensharing, window roles) are auto-disabled on
  Windows; no flag is needed.
- Long CPM build paths can hit the default Windows 260-character path
  limit. Building from a short path (e.g. `C:\src\komai`) avoids this.
- OpenSSL needs to be discoverable by CMake. The
  [Shining Light Productions OpenSSL build](https://slproweb.com/products/Win32OpenSSL.html)
  is a common choice; set `OPENSSL_ROOT_DIR` if CMake can't find it.

```cmd
cmake -S. -Bvar/build/native -DCMAKE_BUILD_TYPE=Release ^
    -DVOIP=OFF
cmake --build var/build/native --parallel
```

### macOS-specific notes

- Install Qt via the official installer and point CMake at it via
  `-DCMAKE_PREFIX_PATH=/path/to/Qt/6.x.x/macos`.
- The Qt installation must include ICNS imageformat support, because
  the app bundle icon is generated from `resources/komai.svg` during
  the build.
- VOIP (GStreamer) typically works on macOS via Homebrew
  (`brew install gstreamer gst-plugins-base gst-plugins-good
  gst-plugins-bad gst-plugins-ugly`), but is untested in this project.
  Pass `-DVOIP=OFF` for a first attempt if anything goes wrong.

```sh
cmake -S. -Bvar/build/native -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$HOME/Qt/6.8.0/macos
cmake --build var/build/native --parallel $(sysctl -n hw.ncpu)
```

### What's likely to go wrong

- **VOIP / GStreamer.** GStreamer 1.20+ is non-trivial on Windows and
  imperfectly packaged on macOS. Disable with `-DVOIP=OFF` for a first
  successful build, then re-enable once the rest works.
- **qtkeychain / KDSingleApplication.** Both build via CPM by default; if
  CPM fails (network, compiler quirks), install them manually and point
  CMake at them via `-DCPM_<package>_USE_LOCAL=ON`.
- **Single-instance support (KDSingleApplication).** Cross-platform in
  principle but has had platform-specific bugs historically.
- **The Snap, Flatpak, and AppImage build helpers in `etc/packaging/`
  are Linux-only.** Only the plain CMake build flow above applies on
  Windows and macOS.

If you get this working, please open an issue or PR with what worked so
this section can be tightened up.

## Debug builds

```sh
just configure-debug
just build
```

This enables debug symbols and generates `compile_commands.json` (for LSP/IDE integration). The output is in the same `var/build/native/` directory.
