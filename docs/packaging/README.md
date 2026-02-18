# Packaging

Komai can be packaged and installed through several methods. This directory documents each one.

| Method | Status | Guide |
|--------|--------|-------|
| [Native build](native.md) | Working | `just build && just install` |
| [AppImage](appimage.md) | Working | `just appimage-build-docker` |
| [Arch Linux](archlinux.md) | Working | PKGBUILD for AUR / local `makepkg` |
| [Flatpak](flatpak.md) | Working | Build locally with `just flatpak-build` |

## Directory layout

Packaging-related source files (manifests, PKGBUILDs, etc.) live in [`etc/packaging/`](../../etc/packaging/), organized by method:

```
etc/packaging/
  appimage/
    AppImageBuilder.yml   appimage-builder manifest
    builder-image         Docker image reference (Renovate-trackable)
    bin/
      build-docker        Docker-based build script
      build-native        Native build script (Ubuntu 25.04+)
    README.md
  archlinux/    PKGBUILD + maintainer notes
  flatpak/      Flatpak manifest (cc.etke.komai.yaml)
```

Build output goes to `var/build/<method>/` (gitignored):

```
var/build/
  native/       CMake native host build
  appimage/     AppImage builder output + bundles
  flatpak/      Flatpak builder output + bundles
```
