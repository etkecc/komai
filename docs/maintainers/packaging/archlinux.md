# Arch Linux

Build and install Komai as an Arch Linux package using the PKGBUILD at [`etc/packaging/archlinux/`](../../../etc/packaging/archlinux/).

## Install from the PKGBUILD

```sh
cd etc/packaging/archlinux
makepkg -si
```

This clones the Komai git repository at the release tag specified in `pkgver`, builds it with CMake, and installs it system-wide.

## How it works

The PKGBUILD is modeled after the [official nheko PKGBUILD](https://gitlab.archlinux.org/archlinux/packaging/packages/nheko) from the Arch repos. It uses `git+https://github.com/etkecc/komai.git#tag=v${pkgver}` as the source, so it pulls from a tagged release -- no pre-packaged tarball needed.

| Stage | What happens |
|-------|-------------|
| `build()` | `cmake -B build -DCMAKE_INSTALL_PREFIX=/usr -DMAN=OFF && cmake --build build` |
| `package()` | `DESTDIR="$pkgdir" cmake --install build` |

## For maintainers

See the [maintainer notes](../../../etc/packaging/archlinux/README.md) for details on updating the PKGBUILD for new releases and differences from the official nheko package.
