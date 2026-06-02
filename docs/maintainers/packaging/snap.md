# Snap

Build Komai as a [Snap](https://snapcraft.io/) package. Snaps are containerized application packages that auto-update and run on most Linux distributions with snapd installed.

All build methods produce a snap for the host architecture: `var/build/snap/komai_<version>_amd64.snap` on x86_64, or `komai_<version>_arm64.snap` on arm64.

## Build methods

### Docker build (recommended)

Works on any Linux distro. Everything runs inside an Ubuntu 24.04 container.

**Prerequisites**: [Docker](https://docs.docker.com/engine/install/)

```sh
just snap-build-docker
```

### Native build with LXD

Runs snapcraft on the host, using [LXD](https://documentation.ubuntu.com/lxd/) for build isolation.

**Prerequisites**: [snapcraft](https://snapcraft.io/docs/snapcraft-overview) and LXD.

```sh
sudo snap install snapcraft --classic
sudo snap install lxd
sudo lxd init --auto
sudo usermod -aG lxd $USER
```

Log out and back in (or reboot) for the group change to take effect, then:

```sh
just snap-build-native
```

### Native build in destructive mode

Runs snapcraft directly on the host without LXD isolation. Best used in disposable environments (VMs, CI) since it installs build dependencies system-wide.

Use this when LXD is not available, e.g. inside a VM where nested containers lack network access.

**Prerequisites**: [snapcraft](https://snapcraft.io/docs/snapcraft-overview) (`sudo snap install snapcraft --classic`)

```sh
sudo just snap-build-native-destructive
```

Requires `sudo` because snapcraft installs build dependencies system-wide in this mode.

## Installing the snap

```sh
just snap-install         # sudo snap install --dangerous var/build/snap/komai_*.snap
just snap-run             # snap run komai
```

The `--dangerous` flag is required for locally-built (unsigned) snaps.

## Justfile recipes

| Recipe | What it does |
|--------|-------------|
| `just snap-build-docker` | Builds Komai snap inside a Docker container (works on any distro) |
| `just snap-build-native` | Builds on the host using snapcraft + LXD for isolation |
| `just snap-build-native-destructive` | Builds on the host without LXD isolation (destructive mode) |
| `just snap-install` | Installs the locally-built snap (`--dangerous` for unsigned snaps) |
| `just snap-run` | Runs the snap-installed Komai |
| `just snap-clean` | Removes the snap build directory (`var/build/snap/`) |

## How it works

All build methods do the same steps:

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
