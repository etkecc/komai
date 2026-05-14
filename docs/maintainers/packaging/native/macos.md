# Native build on macOS

For shared reference (dependencies, CMake flags, CPM), see
[native.md](../native.md).

Verified on Apple Silicon, macOS 26.

## Prerequisites

- macOS 13.3 or newer. Earlier versions don't ship a libc++ with the floating-point overloads of `std::to_chars`, which Komai uses through `std::format`.
- [Qt 6.5+](https://www.qt.io/download) with Base, Declarative, Multimedia, SVG, and Tools modules. Both Homebrew (`brew install qt`) and the Qt online installer are known to work.
- [Python 3](https://www.python.org/downloads/) on `PATH`.
- [Rust](https://rustup.rs/) — `rustup` reads [`rust-toolchain.toml`](../../../../rust-toolchain.toml) and picks the pinned version.
- Xcode 14+ or a recent Apple Clang from Command Line Tools.
- CMake 3.15+.
- **`bash` 4 or newer** (`brew install bash`). Apple ships `/bin/bash` at 3.2; `bin/build/native.sh` uses bash 4+ features (e.g. `BASHPID`) and aborts immediately on the stock shell. The brewed bash is picked up automatically by the script's `#!/usr/bin/env bash` shebang, since Homebrew's bin directory comes earlier on `PATH`.
- [`just`](https://github.com/casey/just) (`brew install just`) — optional but recommended; the rest of this guide drives the build through it.

## Get the source

```sh
git clone https://github.com/etkecc/komai && cd komai
```

## Build

```sh
just build
```

On macOS this configures (if needed), compiles, runs `cmake --install` into a project-local prefix, and invokes `macdeployqt` so the output is a self-contained `.app`. The result is at:

```
var/build/native/dist/komai.app
```

Launch it from Finder, or:

```sh
open var/build/native/dist/komai.app
```

If `cmake` can't find Qt automatically (typical with the Qt online installer), point it at your install once with `just configure`:

```sh
just configure -DCMAKE_PREFIX_PATH=$HOME/Qt/<QT_VER>/macos
just build
```

## VOIP

Disabled by default on macOS. GStreamer is available on Homebrew but the integration is unverified there. To experiment:

```sh
brew install gstreamer gst-plugins-base gst-plugins-good \
    gst-plugins-bad gst-plugins-ugly
just configure -DVOIP=ON
just build
```

## Packaging notes

- `macdeployqt`'s default strip + ad-hoc codesign passes run automatically during `just build`. To suppress them (for example, when the host toolchain's `strip` doesn't understand modern Mach-O load commands, or when the codesign step runs in a sandbox that can't reach `/usr/bin/codesign`), set `-DKOMAI_DEPLOY_TOOL_OPTIONS=-no-strip;-no-codesign` and handle stripping / signing explicitly after install.
- The Snap, Flatpak, and AppImage helpers in `etc/packaging/` don't apply on macOS.

## What's likely to go wrong

- **CMake can't find Qt.** Set `CMAKE_PREFIX_PATH` to your Qt install root (see above).
- **VOIP.** Off by default; only turn on once Qt and Komai itself build cleanly.
- **`just` isn't installed.** Either install it (`brew install just`) or drive the underlying script manually: `bash bin/build/native.sh build`.

If something else trips you up, please open an issue or PR with the symptom and the fix.
