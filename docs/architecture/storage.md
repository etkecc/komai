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

## Storage Backends

- Backend selection is currently runtime-configurable via `KOMAI_DB_BACKEND`.
- Supported backend IDs:
  - `memory` (always available; fallback default when LMDB backend is not built)
  - `lmdb` (default when `KOMAI_DB_ENABLE_LMDB_BACKEND=ON`)
- Compaction is backend-capability-driven (`Backend::supportsCompaction()`):
  enabled for `lmdb` when present, skipped for `memory`.
- Cross-backend data copy used by compaction is implemented in `src/db/Compaction.cpp` (`db::compact(...)`).
- Selection is delegated to `src/db/Factory.cpp`
  (`db::createConfiguredBackendFromEnvironment(...)` and
  `db::createConfiguredBackend(...)`), while backend implementations live under `src/db/`.
- Logical DB names and sync-state keys are centralized in `src/db/Catalog.cpp`
  (`db::catalog::*`) so callers don't hardcode backend-facing names
  (including legacy migration name patterns, sync-state secret key names,
  and composite value codecs such as OLM session and state-event index values).
- Reusable schema/migration primitives (for example, full-resync room DB lists,
  drop helpers, and legacy migration helpers) are centralized in
  `src/db/Schema.cpp` (`db::roomDbsForFullResync(...)`,
  `db::tryDropNamedDbi(...)`, `db::migrateLegacyStateByKeyToStatesKey(...)`,
  `db::migrateLegacyMegolmSessionIndexes(...)`, `db::migrateLegacyOlmShardsV1ToV2(...)`,
  `db::migrateLegacyOlmShardsV2ToUnified(...)`).
- Reusable state-index query helpers are centralized in `src/db/StateIndex.cpp`
  (`db::findStateEventId(...)`, `db::listStateEventIds(...)`, `db::putStateEventId(...)`,
  `db::removeStateEventId(...)`) so callers do not need
  to encode backend cursor/dupsort iteration details.
- Reusable dupsort key->values iteration is centralized in `src/db/DupIndex.cpp`
  (`db::listDupValues(...)`, `db::forEachDupValue(...)`) to avoid repeating cursor
  `Set`/`FirstDup`/`NextDup` loops.
- Reusable key scanning is centralized in `src/db/Scan.cpp`
  (`db::listKeys(...)`, `db::listEntries(...)` including paged iteration, and
  `db::forEachEntry(...)` including paged iteration, plus ordered-entry helpers
  such as `db::firstEntry(...)`, `db::lastEntry(...)`, and
  `db::forEachEntryFromKey(...)` / `db::forEachEntryWithPrefix(...)`) so callers can iterate DB keys/entries
  without cursor boilerplate.
- Cursor-level APIs are intentionally mostly confined to backend internals and
  helper modules (`Scan`, `DupIndex`) so higher-level code can stay backend-neutral.
- Cache DB open options (integer-key / dupsort / comparator) are centralized in
  `src/db/NamePolicy.cpp` (`db::openOptionsForName(...)`,
  `db::openOptionsForGlobal(...)`, `db::openOptionsForRoom(...)`) and consumed via
  `src/db/Open.cpp` (`db::openNamedDbi(...)`, `db::openGlobalDbi(...)`,
  `db::openRoomDbi(...)`); backend APIs consume a unified
  `db::DbiOpenOptions` instead of separate flag/comparator arguments.

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
- `src/Cache.cpp` (database base directory)
- `src/ui/ThemeRegistry.cpp` (external theme search directories)
- `src/MatrixClient.cpp` (curl alt-svc cache file)
- `src/MxcImageProvider.cpp` (media purge + media cache file paths)
- `src/ui/MxcAnimatedImage.cpp` (media cache file paths)
- `src/ui/MxcMediaProxy.cpp` (media cache file paths)
- `src/timeline/TimelineModel.cpp` (downloaded media cache paths)
- `src/main.cpp` (cache/data directory creation and log file path)
- `src/notifications/ManagerWin.cpp` (cached room avatar path)
