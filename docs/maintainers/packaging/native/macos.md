# Native build on macOS

> ⚠️ **Untested by maintainers.** Komai has never been built on macOS
> by the project. The notes below are tentative pointers, not verified
> instructions. PRs welcome.

For shared reference (dependencies, CMake flags, CPM), see
[native.md](../native.md).

## Prerequisites

- [Qt 6.5+](https://www.qt.io/download) with Base, Declarative,
  Multimedia, SVG, and Tools modules, **including ICNS imageformat
  support** (the app bundle icon is generated from
  `resources/komai.svg` at build time).
- [Python 3](https://www.python.org/downloads/) on `PATH`.
- [Rust](https://rustup.rs/) -- `rustup` reads
  [`rust-toolchain.toml`](../../../../rust-toolchain.toml) and picks
  the pinned version.
- Xcode 14+ or a recent Apple Clang from Command Line Tools.
- CMake 3.15+.

## Get the source

```sh
git clone https://github.com/etkecc/komai && cd komai
```

## Configure and build

```sh
cmake -S. -Bvar/build/native -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=$HOME/Qt/<QT_VER>/macos
cmake --build var/build/native --parallel $(sysctl -n hw.ncpu)
```

Adjust `-DCMAKE_PREFIX_PATH` to wherever the Qt installer placed Qt.

## VOIP

GStreamer is available on Homebrew but untested here -- start with
`-DVOIP=OFF`, then add it later:

```sh
brew install gstreamer gst-plugins-base gst-plugins-good \
    gst-plugins-bad gst-plugins-ugly
```

## What's likely to go wrong

- **GStreamer / VOIP.** Build with `-DVOIP=OFF` first.
- **qtkeychain / KDSingleApplication.** Both build via CPM by default;
  if CPM fails, install manually and pass `-DCPM_<package>_USE_LOCAL=ON`.
  KDSingleApplication has had platform-specific bugs historically.
- **Linux-only packaging helpers.** The Snap, Flatpak, and AppImage
  build helpers in `etc/packaging/` don't apply -- only the plain
  CMake flow above does.

If you get a build working, please open an issue or PR with what
worked so this page can be tightened up.
