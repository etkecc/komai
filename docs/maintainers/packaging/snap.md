# Snap

Build Komai as a [Snap](https://snapcraft.io/) package. Snaps are containerized application packages that auto-update and run on most Linux distributions with snapd installed.

## Build

The recommended way is to build inside a Docker container, which works on any Linux distro:

```sh
just snap-build-docker    # Build inside Ubuntu 24.04 container
```

The output file is `var/build/snap/komai_<version>_amd64.snap`.

### Installing the snap

```sh
just snap-install         # sudo snap install --dangerous var/build/snap/komai_*.snap
just snap-run             # snap run komai
```

The `--dangerous` flag is required for locally-built (unsigned) snaps.

## Prerequisites

**Docker build** (recommended): Only [Docker](https://docs.docker.com/engine/install/) is required. Everything else runs inside the container.

**Native build**: Requires [snapcraft](https://snapcraft.io/docs/snapcraft-overview) installed on the host (`sudo snap install snapcraft --classic`).

## Justfile recipes

| Recipe | What it does |
|--------|-------------|
| `just snap-build-docker` | Builds Komai snap inside a Docker container (works on any distro) |
| `just snap-build-native` | Same, but runs directly on the host (requires snapcraft installed) |
| `just snap-install` | Installs the locally-built snap (`--dangerous` for unsigned snaps) |
| `just snap-run` | Runs the snap-installed Komai |
| `just snap-clean` | Removes the snap build directory (`var/build/snap/`) |

## How it works

Both build recipes do the same steps (one inside Docker, the other on the host):

1. **Prepare** -- copies source tree to a temporary directory with `snap/snapcraft.yaml` in place
2. **Build with snapcraft** -- snapcraft uses the `cmake` plugin to configure and compile Komai, with Rust installed via rustup in an `override-build` step
3. **Package** -- snapcraft bundles the compiled application with its runtime dependencies into a `.snap` file

### kde-neon-6 extension

The snap uses the [kde-neon-6 extension](https://documentation.ubuntu.com/snapcraft/stable/reference/extensions/kde-neon-extensions/), which provides the Qt 6.7+ runtime via shared KDE content snaps. This keeps the snap size small (Qt libraries are shared with other KDE/Qt snaps) and solves the Qt version gap -- Ubuntu 24.04 (core24 base) only ships Qt 6.4, but Komai requires Qt 6.5+.

The extension provides Qt6 libraries, QML modules, and KDE Frameworks at both build time (via the SDK snap) and runtime (via the content snap). Only non-Qt dependencies (GStreamer, QtKeychain, libsecret, libproxy) are bundled directly in the snap.

## Status

**Untested.** The snap builds successfully but has not yet been tested at runtime (installation, launch, interface connections). Feedback welcome.

## Notes

- The Docker build uses `--destructive-mode` because snapcraft cannot use LXD/Multipass inside a container.
- The snap uses `strict` confinement with explicit interface plugs for desktop, network, audio, camera, and secret storage access.
- **Emoji fonts**: Like the AppImage, the snap does not bundle an emoji font -- it relies on the host system having one installed.
- The `gstreamer1.0-qt6` package is not available on Ubuntu 24.04, so GStreamer's Qt6 video sink is not included. VoIP audio works; video rendering through GStreamer's Qt6 sink is unavailable.
