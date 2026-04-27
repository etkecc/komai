# 📥 Installation

Komai runs on Linux desktops (x86_64). Pre-built packages are published with each release, and an Arch Linux package is available from the AUR. If you'd rather build it yourself, see 🔨 [Build from source](#-build-from-source).

> **Windows and macOS users:** there are no official builds. Building
> from source on those platforms has never been tested by the maintainers.
> If you'd like to try, see 📄 [Native build — Windows and macOS notes](../maintainers/packaging/native.md#-windows-and-macos-untested)
> for tentative pointers. Contributions to flesh those out are welcome.

## 📦 Pre-built packages

Each [GitHub release](https://github.com/etkecc/komai/releases) ships these formats:

### 🧳 AppImage

A portable single-file bundle that runs on most x86_64 Linux distros without installation. Requires FUSE 2 (`fuse2` on Arch, `libfuse2` on Debian/Ubuntu).

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


## 🐧 Arch Linux (AUR)

A [`komai`](https://aur.archlinux.org/packages/komai) package is available on the AUR. Install with your preferred AUR helper:

```sh
yay -S komai
# or
paru -S komai
```

The AUR package is built from the [PKGBUILD](https://github.com/etkecc/komai/blob/main/etc/packaging/archlinux/PKGBUILD) shipped with the project. For background, see 📄 [Arch Linux packaging](../maintainers/packaging/archlinux.md).


## 🔨 Build from source

If you'd rather build Komai yourself, see 📄 [Native build](../maintainers/packaging/native.md) for dependencies, distro package lists, and CMake flags.
