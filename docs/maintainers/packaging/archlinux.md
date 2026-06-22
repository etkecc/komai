# Arch Linux

Build and install Komai as an Arch Linux package using the PKGBUILD at [`etc/packaging/archlinux/komai/`](../../../etc/packaging/archlinux/komai/).

## Install from the PKGBUILD

```sh
cd etc/packaging/archlinux/komai
makepkg -si
```

This clones the Komai git repository at the release tag specified in `pkgver`, builds it with CMake, and installs it system-wide.

Because the PKGBUILD uses `cmake --install` during `package()`, newly installed helper binaries such as `komai-mcp` are picked up automatically once the tagged release contains the matching CMake install rules.

## How it works

The PKGBUILD is modeled after the [official nheko PKGBUILD](https://gitlab.archlinux.org/archlinux/packaging/packages/nheko) from the Arch repos. It uses `git+https://github.com/etkecc/komai.git#tag=v${pkgver}` as the source, so it pulls from a tagged release -- no pre-packaged tarball needed.

| Stage | What happens |
|-------|-------------|
| `prepare()` | Removes `rust-toolchain.toml` so the build follows the system Rust toolchain instead of upstream's pinned channel. See the [packaging README](../../../etc/packaging/archlinux/komai/README.md#rust-toolchain-handling) for the rationale. |
| `build()` | `cmake -B build -S komai <options> && cmake --build build` |
| `package()` | `DESTDIR="$pkgdir" cmake --install build` |

The CMake options the PKGBUILD passes are:

- `-DCMAKE_BUILD_TYPE=None` (defer to Arch's `CXXFLAGS`/`CFLAGS`)
- `-DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_INSTALL_LIBDIR=lib`
- `-DBUILD_TESTING=OFF`
- `-DMAN=ON` (build the asciidoctor manpage)
- `-DCPM_USE_LOCAL_PACKAGES=ON` (use system `qt6keychain` and `kdsingleapplication`, listed under `depends`)

## For maintainers

See the [maintainer notes](../../../etc/packaging/archlinux/komai/README.md) for details on updating the PKGBUILD for new releases and differences from the official nheko package.
