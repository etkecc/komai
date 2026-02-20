# Storage Architecture

This document describes Komai's path construction and storage layout internals.

## Centralized Path Helper

All storage-path construction is centralized in:

- `src/Paths.h`
- `src/Paths.cpp`

Design rules:

- Use explicit XDG generic roots with one app segment (`.../komai`).
- Avoid app/org-dependent double nesting (for example `.../komai/komai`).
- Keep profile normalization consistent across storage concerns.

## Base Directories

- `app_paths::config::root()` -> `QStandardPaths::GenericConfigLocation + "/komai"`
- `app_paths::data::root()` -> `QStandardPaths::GenericDataLocation + "/komai"`
- `app_paths::cache::root()` -> `QStandardPaths::GenericCacheLocation + "/komai"`

## Profile Paths

- `app_paths::config::profileConfigFile(profileId)` -> `config.yml`
- `app_paths::config::profileStateFile(profileId)` -> `state.yml`
- `app_paths::config::profileSessionFile(profileId)` -> `session.yml`
- `app_paths::config::profileSecretsFile(profileId)` -> `secrets.yml`

`app_paths::normalizedProfileId(profileId)` maps empty/default to `default`.

## Data/Cache Paths

- `app_paths::data::dbRoot(profileId)` -> `~/.local/share/komai/profiles/<profile-id>/db`
- `app_paths::data::databaseDirectory(userId, profileId)` -> `.../db/<hash>`
- `app_paths::cache::mediaDirectory(profileId)` -> `~/.cache/komai/profiles/<profile-id>/media_cache`
- `app_paths::cache::mediaMediaDirectory(profileId)` -> `~/.cache/komai/profiles/<profile-id>/media_cache/media`
- `app_paths::cache::mediaFileForMxc(profileId, mxcId, suffix)` -> final media-cache file path
- `app_paths::cache::mediaMediaFileForMxc(profileId, mxcId, suffix)` -> final media-subdir cache file path
- `app_paths::cache::mediaThumbnailFileForMxc(profileId, mxcId, size, crop, radius)` -> final thumbnail cache file path
- `app_paths::cache::roomNotificationAvatarFile(profileId, roomId)` -> final notification avatar cache file path
- `app_paths::cache::logFile(profileId)` -> `~/.cache/komai/profiles/<profile-id>/komai.log`
- `app_paths::cache::altSvcCacheFile(profileId)` -> `~/.cache/komai/profiles/<profile-id>/curl_alt_svc_cache.txt`
- `app_paths::data::userThemesDirectory()` -> `~/.local/share/komai/themes`
- `app_paths::data::themeSearchDirectories()` -> `standardLocations(GenericDataLocation) + "/komai/themes"`
- `app_paths::encodedIdComponent(value)` -> URL-safe Base64 without `=`

Database hash:

- `hash = hex(sha256(user_id))`

## Prefixes

Filesystem prefixes:

- settings root: `~/.config/komai/profiles/`
- data root: `~/.local/share/komai/profiles/`
- cache root: `~/.cache/komai/profiles/`

Secret-store key prefixes:

- `komai.<profile_hash>.settings.`
- `komai.<profile_hash>.local_crypto.`
- `komai.<profile_hash>.matrix.`

## Main Call Sites

- `src/UserSettingsPage.cpp` (profile YAML files)
- `src/Cache.cpp` (LMDB base directory)
- `src/ui/ThemeRegistry.cpp` (external theme search directories)
- `src/MatrixClient.cpp` (curl alt-svc cache file)
- `src/MxcImageProvider.cpp` (media purge + media cache file paths)
- `src/ui/MxcAnimatedImage.cpp` (media cache file paths)
- `src/ui/MxcMediaProxy.cpp` (media cache file paths)
- `src/timeline/TimelineModel.cpp` (downloaded media cache paths)
- `src/main.cpp` (cache/data directory creation and log file path)
- `src/notifications/ManagerWin.cpp` (cached room avatar path)
