# Snap packaging

This directory contains the Snap build infrastructure for Komai.

| File | Purpose |
|------|---------|
| `snap/snapcraft.yaml` | snapcraft manifest (core24, kde-neon-6 extension) |
| `builder-image` | Docker image reference (`docker.io/ubuntu:24.04`), tracked by Renovate |
| `bin/build-docker` | Builds the snap inside a Docker container (works on any distro) |
| `bin/build-native` | Builds the snap natively (requires snapcraft installed) |

For build instructions and details, see [docs/maintainers/packaging/snap.md](../../../docs/maintainers/packaging/snap.md).
