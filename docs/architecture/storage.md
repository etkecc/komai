# Storage Architecture

This document describes Komai's current on-disk layout and the helper layers
that construct those paths.

## Path Helpers

- `src/profile/Paths.h` / `src/profile/Paths.cpp`
  Builds XDG-based config/data/cache roots, profile directories, media-cache
  paths, log paths, and HTTP cache paths.
- `src/matrix/backend/MatrixSdkPaths.h` / `src/matrix/backend/MatrixSdkPaths.cpp`
  Exposes the derived matrix-sdk subpaths to C++/QML callers.
- `src/rust/src/matrix_backend/mod.rs`
  Derives the matrix-sdk storage layout used by the Rust runtime.

## Base Roots

- `app_paths::config::root()` -> `QStandardPaths::GenericConfigLocation + "/komai"`
- `app_paths::data::root()` -> `QStandardPaths::GenericDataLocation + "/komai"`
- `app_paths::cache::root()` -> `QStandardPaths::GenericCacheLocation + "/komai"`

## Profile Files

- `app_paths::config::profileConfigFile(profileId)` -> `.../profiles/<profile-id>/config.yml`
- `app_paths::config::profileStateFile(profileId)` -> `.../profiles/<profile-id>/state.yml`
- `app_paths::config::profileSessionFile(profileId)` -> `.../profiles/<profile-id>/session.yml`
- `app_paths::config::profileSecretsFile(profileId)` -> `.../profiles/<profile-id>/secrets.yml`

`app_paths::normalizedProfileId(profileId)` maps empty/default to `default`.

## Current Data Layout

Profile-scoped persistent data now lives directly under the profile roots.
There is no LMDB backend or `db/<encoded-user-id>/` layout in the current tree.

Data root:

- `app_paths::data::profileDirectory(profileId)` -> `~/.local/share/komai/profiles/<profile-id>`
- matrix-sdk data root -> `.../matrix-sdk`
- matrix-sdk state store -> `.../matrix-sdk/state-store`

Cache root:

- `app_paths::cache::profileDirectory(profileId)` -> `~/.cache/komai/profiles/<profile-id>`
- matrix-sdk cache root -> `.../matrix-sdk/cache`
- matrix-sdk event cache -> `.../matrix-sdk/cache/event-cache`
- matrix-sdk media cache -> `.../matrix-sdk/cache/media-cache`
- app-managed media cache root -> `.../media`
- shared app media cache -> `.../media/shared`
- room app media cache -> `.../media/rooms/<base64url(room-id)>`
- log directory -> `.../logs`
- HTTP cache directory -> `.../http`

App-managed media files use `app_paths::encodedIdComponent(...)` for cache-safe
MXC and room IDs.

## Current Storage Model

- Persistent Matrix room/account/encryption state is handled by the Rust
  `matrix-sdk` SQLite-backed store.
- The Rust runtime also keeps its own cache tree under the profile cache root.
- Komai still keeps a separate app-managed media cache under `~/.cache/.../media`
  for downloaded thumbnails/full media that the UI can inspect and purge.
- `Komai::localCacheInfo()` is the QML-facing summary used by the Settings UI.
- `profile_manager::deleteProfile(...)` removes the config, data, and cache roots
  for the target profile recursively.

## Main Call Sites

- `src/matrix/backend/MatrixBackendBridge.cpp`
  Passes profile data/cache roots into Rust.
- `src/matrix/backend/MatrixSdkPaths.cpp`
  Exposes derived matrix-sdk paths back to C++/QML.
- `src/providers/MxcImageProvider.cpp`
  Uses app media-cache helpers for thumbnails/full media and purge scheduling.
- `src/ui/MxcMediaProxy.cpp`
  Uses the app media cache for downloaded playback buffers.
- `src/ui/KomaiGlobalObject.cpp`
  Builds the Settings "Local cache" summary.
- `src/profile/ProfileManager.cpp`
  Deletes whole profile config/data/cache roots.
