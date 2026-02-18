# Arch Linux packaging

PKGBUILD for building Komai from source on Arch Linux. Intended for submission to the [AUR](https://aur.archlinux.org/).

For user-facing install instructions, see [docs/packaging/archlinux.md](../../../docs/packaging/archlinux.md).

## How it works

The PKGBUILD clones the Komai git repository at a release tag (e.g., `v0.1.0`) and builds it with CMake — the same as the [official nheko PKGBUILD](https://gitlab.archlinux.org/archlinux/packaging/packages/nheko), just rebranded.

| Stage | What happens |
|-------|-------------|
| `build()` | CMake configure + build with `-DCMAKE_INSTALL_PREFIX=/usr -DMAN=OFF` |
| `package()` | `cmake --install` with `DESTDIR` to stage files into the package directory |

## Differences from the official nheko package

| | nheko (official) | komai |
|---|---|---|
| **Source** | nheko git repo | Komai git repo |
| **Generator** | Ninja (`-GNinja`) | Unix Makefiles (default) — [no meaningful difference](#why-not-ninja) |
| **Man page** | Built with asciidoc | Skipped (`-DMAN=OFF`) |
| **Binary name** | `nheko` | `komai` |
| **Extra makedeps** | `ninja`, `asciidoc` | `python` (theme generation at build time) |
| **`provides`/`conflicts`** | — | `provides=('nheko')`, `conflicts=('nheko')` |

### Why not Ninja?

Benchmarked on a 24-core Ryzen 9: clean build is ~71s with either Unix Makefiles or Ninja — no meaningful difference when compilation is fully parallelized across all cores. Sticking with Make avoids an extra build dependency for packagers.

## Updating the PKGBUILD

When releasing a new Komai version:

1. Update `pkgver` to the new version (e.g., `0.2.0`)
2. Reset `pkgrel` to `1`
3. Update `sha256sums` (or keep `SKIP` for development)
4. Test with `makepkg -si`
5. Push to AUR: copy `PKGBUILD` to the AUR git repo, generate `.SRCINFO` with `makepkg --printsrcinfo > .SRCINFO`, commit and push

## Testing locally

```sh
# Build the package (from this directory)
makepkg -si

# Or in a clean chroot (recommended for verifying makedeps)
extra-x86_64-build
```
