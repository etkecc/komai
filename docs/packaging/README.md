# Packaging

Komai can be packaged and installed through several methods. This directory documents each one.

| Method | Status | Guide |
|--------|--------|-------|
| [Native build](native.md) | Working | `just build && just install` |
| [Flatpak](flatpak.md) | Working | Build locally with `just flatpak-build` |
| [Arch Linux](archlinux.md) | Working | PKGBUILD for AUR / local `makepkg` |

## Directory layout

Packaging-related source files (manifests, PKGBUILDs, etc.) live in [`etc/packaging/`](../../etc/packaging/), organized by method:

```
etc/packaging/
  archlinux/    PKGBUILD + maintainer notes
  flatpak/      Flatpak manifest (cc.etke.komai.yaml)
```

Build output goes to `var/build/<method>/` (gitignored):

```
var/build/
  native/       CMake native host build
  flatpak/      Flatpak builder output + bundles
```
