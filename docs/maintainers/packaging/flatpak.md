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
2. Builds dependency modules from source (LMDB, olm, spdlog, etc.)
3. Builds Komai itself from the local source tree
4. Packages everything into a Flatpak bundle

Komai uses bundled `mtxclient` by default. For Flatpak, the manifest pre-fetches the
`mtxclient` source as `.deps/mtxclient` and points CMake there via
`-DFETCHCONTENT_SOURCE_DIR_MATRIXCLIENT=.deps/mtxclient` so builds stay offline-friendly.

The `flatpak-builder` tool caches each module, so subsequent builds only rebuild what changed. The cache lives at `var/build/flatpak/.flatpak-builder/`.

## App ID

The Flatpak uses app ID `cc.etke.komai`, matching the desktop file, appdata, and icon names used in the CMake build.

## Notes

- The first build downloads the KDE SDK/runtime (~1.5 GB) and builds all dependencies from source. Subsequent builds are much faster due to caching.
- The `just flatpak-build` recipe automatically adds the Flathub remote at the user level if it's missing.
- The Flatpak is installed per-user (`--user`), not system-wide.
