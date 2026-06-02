# AppImage packaging

This directory contains the AppImage build infrastructure for Komai.

| File | Purpose |
|------|---------|
| `AppImageBuilder.yml` | appimage-builder manifest (Qt6, GStreamer, Ubuntu Resolute repos) |
| `builder-image` | Docker image reference (`docker.io/ubuntu:26.04`), tracked by Renovate |
| `bin/build-docker` | Builds the AppImage inside a Docker container (works on any distro) |
| `bin/build-native` | Builds the AppImage natively (requires Ubuntu 26.04+ with deps installed) |

For build instructions and details, see [docs/maintainers/packaging/appimage.md](../../../docs/maintainers/packaging/appimage.md).
