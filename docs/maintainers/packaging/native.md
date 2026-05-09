# Native build

Build and install Komai directly on your system using CMake.

## Pick your platform

| Platform | Status | Guide |
|----------|--------|-------|
| Linux (x86_64) | Primary supported platform; exercised by CI | [native/linux.md](native/linux.md) |
| Windows | Builds and runs without VOIP (verified May 2026 on Windows 10) | [native/windows.md](native/windows.md) |
| macOS | Untested by maintainers; tentative pointers only | [native/macos.md](native/macos.md) |

The platform pages are self-contained: each covers prerequisites,
package install commands, configure, build, and run for that platform.
The rest of this page is shared reference.

## Common prerequisites

- [just](https://github.com/casey/just) command runner (Linux only;
  optional but recommended)
- **Python 3** (for theme generation and emoji data generation at
  build time)
- **CMake** 3.15+
- **C++20 compiler**: GCC 11.3+, Clang 16+, or MSVC 19.13+
- **Rust** (rustc + cargo; `rustup` recommended) for the matrix-sdk
  runtime, settings/theme/syntax helpers, and supporting Rust crates.
  Komai pins a specific toolchain via
  [`rust-toolchain.toml`](../../../rust-toolchain.toml); CMake/Corrosion
  use that pinned version automatically.

## Dependencies

### Required

| Dependency | Minimum version | Notes |
|------------|-----------------|-------|
| [Qt6](https://www.qt.io/) | 6.5 | Base, Declarative, Multimedia, SVG, Tools |
| [CMake](https://cmake.org/) | 3.15 | |
| [Python 3](https://www.python.org/) | | Theme generation and emoji data generation at build time |
| [qtkeychain](https://github.com/frankosterfeld/qtkeychain) | 0.12 | Credential storage |
| [KDSingleApplication](https://github.com/KDAB/KDSingleApplication) | 1.0 | Single-instance support |

### Optional

| Dependency | Purpose | CMake flag to disable |
|------------|---------|-----------------------|
| [GStreamer](https://gitlab.freedesktop.org/gstreamer) 1.20+ | VOIP (voice & video calls) | `-DVOIP=OFF` |
| XCB, XCB-EWMH | X11 screensharing and window roles (Linux only; auto-disabled elsewhere) | `-DX11=OFF` |

## System packages vs CPM downloads

By default, most C++ dependencies are downloaded and built by
[CPM.cmake](https://github.com/cpm-cmake/CPM.cmake). Distro packagers
can use system packages instead:

```sh
# Use system packages for all dependencies
cmake -S. -Bvar/build/native -DCPM_USE_LOCAL_PACKAGES=ON

# Use a system package for a single library
cmake -S. -Bvar/build/native -DCPM_Qt6Keychain_USE_LOCAL=ON
```

See [CPM.cmake options](https://github.com/cpm-cmake/CPM.cmake#options)
for details.
