# Native build on Linux

> 💡 **You may not need to build from source.** Komai ships pre-built
> **AppImage**, **Flatpak**, and **Snap** bundles with every
> [GitHub release](https://github.com/etkecc/komai/releases), and
> distro-native packages are available for some distros. See 📄
> [Installation](../../../user-guide/installation.md) for the full
> list and install commands. Build from source for development, custom
> CMake flags, or distros that don't yet have a package.

Build and install Komai directly on a Linux system using CMake.
This is the primary supported platform; CI exercises this build.
For shared reference (dependencies, CMake flags, CPM), see
[native.md](../native.md).

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

## Distro-specific package lists

### Arch Linux

```sh
sudo pacman -S --needed --asdeps qt6-base qt6-declarative qt6-tools qt6-multimedia qt6-svg \
    cmake gcc fontconfig python rust \
    openssl \
    qtkeychain-qt6 kdsingleapplication litehtml
```

For VOIP (skip if you pass `-DVOIP=OFF`):

```sh
sudo pacman -S --needed --asdeps gst-plugins-base gst-plugins-good gst-plugins-bad \
    libnice gst-plugin-qml6 gst-plugin-pipewire
```

Each plugin is load-bearing: `gst-plugins-bad` provides `webrtcbin`
(gates calls), `gst-plugin-qml6` is the Qt6 GL video sink (without it
calls fail to render), and `gst-plugin-pipewire` provides `pipewiresrc`
for Wayland screen-share capture.

### Debian 13+ / Ubuntu 24.04+

```sh
sudo apt install -y build-essential cmake pkg-config python3 cargo rustc \
    libssl-dev \
    libkdsingleapplication-qt6-dev \
    qt6-base-dev qt6-tools-dev qt6-svg-dev qt6-multimedia-dev \
    qt6-declarative-dev qtkeychain-qt6-dev qt6-base-private-dev \
    qt6-declarative-private-dev
```

For VOIP (skip if you pass `-DVOIP=OFF`):

```sh
sudo apt install -y libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev libnice-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-nice gstreamer1.0-qt6 \
    gstreamer1.0-pipewire
```

If your distro's `cargo`/`rustc` doesn't match the pin in
[`rust-toolchain.toml`](../../../../rust-toolchain.toml), `rustup`
reads the pin and installs the right version when run inside the
repo. To build against whatever `rustc`/`cargo` the distro ships,
delete `rust-toolchain.toml` before configuring -- Komai isn't
coupled to a specific toolchain. (The Arch PKGBUILD takes this
route.)

## Debug builds

```sh
just configure-debug
just build
```

This enables debug symbols and generates `compile_commands.json` (for
LSP/IDE integration). The output is in the same `var/build/native/`
directory.
