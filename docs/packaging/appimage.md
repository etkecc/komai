# 📦 AppImage

Build Komai as a portable [AppImage](https://appimage.org/) bundle. The AppImage is a single executable file that runs on most x86_64 Linux distributions without installation.

## 🚀 Build

The recommended way is to build inside a Docker container, which works on any Linux distro:

```sh
just appimage-build-docker    # 🐳 Build inside Ubuntu 25.04 container (~10 min first run)
```

The output file is `var/build/appimage/komai-latest-x86_64.AppImage`.

### ▶️ Running the AppImage

On most distros you just need FUSE 2 installed (e.g. `sudo pacman -S fuse2` on Arch):

```sh
chmod +x var/build/appimage/komai-latest-x86_64.AppImage
./var/build/appimage/komai-latest-x86_64.AppImage
```

## 📋 Prerequisites

🐳 **Docker build** (recommended): Only [Docker](https://docs.docker.com/engine/install/) is required. Everything else runs inside the container.

🔧 **Native build** (Ubuntu 25.04+ only): Requires a native build toolchain (see [Native build](native.md)) and [`appimage-builder`](https://appimage-builder.readthedocs.io/) (`pip3 install appimage-builder`).

## 🍳 Justfile recipes

| Recipe | What it does |
|--------|-------------|
| `just appimage-build-docker` | 🐳 Builds Komai and produces an `.AppImage` inside a Docker container (works on any distro) |
| `just appimage-build-native` | 🔧 Same, but runs directly on the host (requires Ubuntu 25.04+ with `appimage-builder` installed) |
| `just appimage-clean` | 🧹 Removes the AppImage build directory (`var/build/appimage/`) |

## ⚙️ How it works

Both recipes do the same three steps (one inside Docker, the other on the host):

1. 🔨 **Compile** -- builds Komai from source using CMake with `-DCMAKE_INSTALL_PREFIX=/usr`
2. 📂 **Install into AppDir** -- installs the binary and resources into `var/build/appimage/AppDir/usr/`
3. 📦 **Bundle with appimage-builder** -- uses the manifest at [`etc/packaging/appimage/AppImageBuilder.yml`](../../etc/packaging/appimage/AppImageBuilder.yml) to pull runtime dependencies (Qt6, GStreamer, etc.) from Ubuntu 25.04 (Plucky) apt repos and package everything into a self-contained AppImage

The manifest pulls Qt6 libraries and QML modules, GStreamer plugins (for VoIP/media), and system libraries from Ubuntu Plucky's apt repos. The resulting AppImage ships its own Qt6 runtime and doesn't depend on the host system's Qt version.

## 🤔 Why Ubuntu 25.04?

`appimage-builder` pulls runtime libraries from Ubuntu apt repos to bundle into the AppImage. Komai requires Qt 6.5+, and Ubuntu 25.04 (Plucky) is the first Ubuntu release to ship Qt 6.8. Earlier versions (24.04 and below) only have Qt 6.4.

## 📝 Notes

- 🐳 The Docker build mounts the source tree read-only (`-v /src:ro`) and writes output to `var/build/appimage/`.
- ⏳ The first Docker run downloads the Ubuntu 25.04 image (~80 MB) and installs build dependencies (~1.5 GB). Subsequent runs reuse the Docker layer cache if the image hasn't changed.
- ⏭️ The `--skip-test` flag is used because `appimage-builder`'s test step requires Docker-in-Docker and a running X server.
- 💻 The AppImage is built for **x86_64** only. ARM builds are not currently supported.
- 😀 **Emoji fonts**: The AppImage does not bundle an emoji font — it relies on the host system having one installed (e.g. `noto-fonts-emoji` on Arch, `fonts-noto-color-emoji` on Debian/Ubuntu). Most modern distros ship one by default. The app auto-detects the best available emoji font at startup.
