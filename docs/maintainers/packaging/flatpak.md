# Flatpak

Build and install Komai as a [Flatpak](https://flatpak.org/) application. The Flatpak runs in a sandboxed environment with its own dependencies, independent of your system packages.

## Prerequisites

- [Flatpak](https://flatpak.org/setup/) installed on your system
- [`flatpak-builder`](https://docs.flatpak.org/en/latest/flatpak-builder.html) installed (e.g. `sudo apt install flatpak-builder` or `sudo pacman -S flatpak-builder`)

## Build, install, and run

```sh
just flatpak-build     # Build the Flatpak bundle (~3 min, first run downloads dependencies)
just flatpak-install   # Install the bundle locally
just flatpak-run       # Run Komai
```

## Justfile recipes

| Recipe | What it does |
|--------|-------------|
| `just flatpak-build` | Builds all dependencies from source in a sandbox, then builds Komai, produces a `.flatpak` bundle |
| `just flatpak-install` | Installs the built bundle as a user Flatpak |
| `just flatpak-run` | Runs the installed Flatpak (`flatpak run cc.etke.komai`) |
| `just flatpak-clean` | Removes the Flatpak build directory (`var/build/flatpak/`) |

## How it works

The Flatpak manifest at [`etc/packaging/flatpak/cc.etke.komai.yaml`](../../../etc/packaging/flatpak/cc.etke.komai.yaml) works similarly to a Dockerfile:

1. Starts from the KDE Platform runtime (provides Qt6, KDE Frameworks, GStreamer, etc.)
2. Builds dependency modules from source (libevent, coeurl, spdlog, etc.)
3. Builds Komai itself from the local source tree
4. Packages everything into a Flatpak bundle

Komai uses [CPM.cmake](https://github.com/cpm-cmake/CPM.cmake) to download most C++
dependencies, while Cargo resolves the Rust `matrix-sdk` runtime crates. For Flatpak,
the manifest pre-fetches the CPM source trees it still needs under `.deps/` and points
CMake there via `-DFETCHCONTENT_SOURCE_DIR_*` flags so builds stay offline-friendly.

Komai also generates runtime emoji JSON during the build. Since the Flatpak build sandbox
has no network access, `just flatpak-build` runs `just emoji-fetch` first on the host and
the manifest copies `var/emoji/cache/` into the source tree so the in-sandbox generation
step uses the pre-fetched Unicode/CLDR cache instead of downloading anything itself.

The `flatpak-builder` tool caches each module, so subsequent builds only rebuild what changed. The cache lives at `var/build/flatpak/.flatpak-builder/`.

## App ID

The Flatpak uses app ID `cc.etke.komai`, matching the desktop file, appdata, and icon names used in the CMake build.

## Notes

- The first build downloads the KDE SDK/runtime (~1.5 GB) and builds all dependencies from source. Subsequent builds are much faster due to caching.
- The `just flatpak-build` recipe automatically adds the Flathub remote at the user level if it's missing.
- The `just flatpak-build` recipe also refreshes the local emoji cache before invoking `flatpak-builder`.
- The Flatpak is installed per-user (`--user`), not system-wide.
