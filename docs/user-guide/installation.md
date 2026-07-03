# 📥 Installation

Komai runs on Linux (`x86_64` and `arm64`), Windows (`x64`), and macOS (`arm64`, Apple Silicon) desktops. We offer [pre-built packages](#pre-built-packages), [distro packages](#distro-packages) (for some distros) and the ability to [🔨 Build from source](#-build-from-source).

## 📦 Pre-built packages

Each [GitHub release](https://github.com/etkecc/komai/releases) ships these formats:

### 🧳 AppImage

A portable single-file bundle that runs on most x86_64 and arm64 Linux distros without installation (pick the `x86_64` or `aarch64` build for your CPU). Requires FUSE 2 (`fuse2` on Arch, `libfuse2` on Debian/Ubuntu).

```sh
# Download komai-*.AppImage from the release page, then:
chmod +x komai-*.AppImage
./komai-*.AppImage
```

For background, see 📄 [AppImage packaging](../maintainers/packaging/appimage.md).

### 📦 Flatpak

A sandboxed application that runs alongside system packages. Requires [Flatpak](https://flatpak.org/setup/).

```sh
# Download komai-*.flatpak from the release page, then:
flatpak install --user ./komai-*.flatpak
flatpak run cc.etke.komai
```

For background, see 📄 [Flatpak packaging](../maintainers/packaging/flatpak.md).

### 🥡 Snap

A containerized package for distros with snapd installed.

> ⚠️ The Snap is currently **untested at runtime** -- feedback welcome.

```sh
# Download komai_*.snap from the release page, then:
sudo snap install --dangerous komai_*.snap
snap run komai
```

The `--dangerous` flag is required because the locally-downloaded snap is not signed by the Snap Store.

For background, see 📄 [Snap packaging](../maintainers/packaging/snap.md).

### 🪟 Windows ZIP (no installer)

A portable ZIP for Windows 10 (22H2+) and later on `x64`. Extract anywhere and run `komai.exe` -- no installer, no admin rights required.

Download `komai-*-windows-x64-no-installer.zip` from the release page, extract the archive (right-click -> *Extract All...* in Explorer, or use any zip tool), then run `komai.exe` from inside the extracted folder.

The first launch shows a **"Windows protected your PC"** SmartScreen warning because Komai isn't code-signed. Click **More info** -> **Run anyway**. Subsequent launches don't re-prompt.

What's not in the Windows build:

- **Legacy 1:1 calls.** The GStreamer-based [legacy call](features/legacy-calls.md) stack is excluded (`-DVOIP=OFF`). [Element Call](features/element-call.md) voice/video is included.
- **Auto-update.** Each release is a fresh ZIP download from [GitHub Releases](https://github.com/etkecc/komai/releases).
- **Windows on ARM.** Only `x64` is available as a pre-built binary right now. Building on ARM has not yet been tested.

For background, see 📄 [Native build on Windows](../maintainers/packaging/native/windows.md).

### 🍏 macOS DMG (Apple Silicon)

A `.dmg` for macOS 13.3 and later on `arm64` (Apple Silicon). The build is unsigned and not notarized, so the first launch needs an extra step; after that, the app opens normally.

Download `komai-*-macos-arm64.dmg` from the release page, open it (double-click in Finder, or `open <file>` in Terminal), and drag `komai.app` into the **Applications** folder.

The first launch shows a Gatekeeper warning because Komai isn't code-signed or notarized:

- **macOS 13 (Ventura) or 14 (Sonoma):** in Finder, right-click `komai.app` in **Applications**, choose **Open**, then click **Open** in the dialog.
- **macOS 15 (Sequoia) or later:** double-click `komai.app` and click **Done** in the warning dialog. Then open **System Settings**, go to **Privacy & Security**, scroll to the bottom, click **Open Anyway** next to the komai.app entry, and confirm.

Subsequent launches open normally.

What's not in the macOS build:

- **Legacy 1:1 calls.** The GStreamer-based [legacy call](features/legacy-calls.md) stack is excluded (`-DVOIP=OFF`). [Element Call](features/element-call.md) voice/video is included.
- **Auto-update.** Each release is a fresh DMG download from [GitHub Releases](https://github.com/etkecc/komai/releases).
- **Intel Macs.** Only `arm64` is available as a pre-built binary. Building on Intel from source still works; see 📄 [Native build on macOS](../maintainers/packaging/native/macos.md).
- **Code-signing / notarization.** Komai is shipped unsigned, hence the first-launch dance.

For background, see 📄 [Native build on macOS](../maintainers/packaging/native/macos.md).


## 🐧 Distro packages

Komai is also available through distro-native package channels. These are listed below as they're added; if you package Komai for a distro that isn't listed here yet, please open a PR.

### Arch Linux (AUR)

A [`komai`](https://aur.archlinux.org/packages/komai) package is available on the AUR. Install with your preferred AUR helper. Example:

```sh
rua install komai
```

The AUR package is built from the [PKGBUILD](https://github.com/etkecc/komai/blob/main/etc/packaging/archlinux/komai/PKGBUILD) shipped with the project. For background, see 📄 [Arch Linux packaging](../maintainers/packaging/archlinux.md).


## 🔨 Build from source

If you'd rather build Komai yourself, see 📄 [Native build](../maintainers/packaging/native.md) for dependencies, distro package lists, and CMake flags.
