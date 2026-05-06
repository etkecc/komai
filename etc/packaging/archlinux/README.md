# Arch Linux packaging

PKGBUILD for building Komai from source on Arch Linux. Intended for submission to the [AUR](https://aur.archlinux.org/).

For user-facing install instructions, see [docs/maintainers/packaging/archlinux.md](../../../docs/maintainers/packaging/archlinux.md).

## How it works

The PKGBUILD clones the Komai git repository at a release tag (e.g., `v0.1.0`) and builds it with CMake — the same as the [official nheko PKGBUILD](https://gitlab.archlinux.org/archlinux/packaging/packages/nheko), just rebranded.

| Stage | What happens |
|-------|-------------|
| `prepare()` | Removes `rust-toolchain.toml` so the build follows the distro's Rust toolchain. See [Rust toolchain handling](#rust-toolchain-handling) below. |
| `build()` | CMake configure + build with the options described below |
| `package()` | `cmake --install` with `DESTDIR` to stage files into the package directory |

That means additional installed binaries, such as `komai-mcp`, are packaged automatically as long as the release tag being built includes the corresponding CMake install rules.

### Notable CMake options

- `-DMAN=ON` builds the asciidoctor man page (the `asciidoctor` makedep covers this).
- `-DCPM_USE_LOCAL_PACKAGES=ON` makes CPM honour the system `qt6keychain` and `kdsingleapplication` packages (listed under `depends`) instead of fetching and rebuilding them.

### Rust toolchain handling

Upstream Komai pins a specific rustup channel via [`rust-toolchain.toml`](../../../rust-toolchain.toml) for development, CI, and Flatpak reproducibility. A distro package should follow the distro's Rust toolchain instead, so `prepare()` removes the file. That single removal covers both surfaces that would otherwise force the pinned version on the user:

- **CMake.** Komai's CMakeLists.txt only parses `rust-toolchain.toml` (and pins Corrosion to its channel) when the file exists, so removal makes Corrosion fall back to whichever `rustc`/`cargo` the `rust` makedep provides.
- **`rustup`'s shim.** When the `rust` makedep is satisfied by `rustup` rather than Arch's `rust` package, `rustup`'s shim respects `rust-toolchain.toml` independently of CMake and would auto-install the pinned channel mid-build (~250MB of unwanted downloads). With the file gone the shim has nothing to pin to.

On systems where `rust` provides a plain `rustc`/`cargo` (Arch's `rust` package), the removal is a harmless no-op.

## Differences from the official nheko package

| | nheko (official) | komai |
|---|---|---|
| **Source** | nheko git repo | Komai git repo |
| **Generator** | Ninja (`-GNinja`) | Unix Makefiles (default) — [no meaningful difference](#why-not-ninja) |
| **Man page** | Built with asciidoc | Skipped (`-DMAN=OFF`) |
| **Binary name** | `nheko` | `komai` |
| **Extra makedeps** | `ninja`, `asciidoc` | `python` (theme generation and emoji pipeline at build time) |

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

The `source=` line fetches the git tag from GitHub, so `makepkg` validates the **tagged release**, not the working tree. Use it when you want to verify that a published tag builds cleanly as an Arch package:

```sh
# Build from this directory. Builds the tag referenced by `pkgver`.
makepkg -si

# Or in a clean chroot (recommended for verifying makedeps)
extra-x86_64-build
```

To test work-in-progress changes *before* tagging a release, build from the local tree instead:

```sh
# From the repository root
just build                                           # build the binary
cmake --install var/build/native --prefix /tmp/check # stage files into /tmp/check for inspection
```

`just install` is also available and will prompt before writing into a system prefix.
