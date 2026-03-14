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
- `app_paths::data::databaseDirectory(userId, profileId)` -> `.../db/<encoded-user-id>`
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

Database user-id component:

- `<encoded-user-id>` is user-id UTF-8 escaped to a cross-platform-safe ASCII
  component: bytes outside `[A-Za-z0-9._@+-]` are encoded as `%HH`
  (uppercase hex), and trailing `.` is encoded as `%2E`.

## Storage Backends

- Supported backend IDs:
  - `memory` (always available; fallback default when no persistent backend is built; ephemeral)
  - `lmdb` (default when available)
- Backends expose persistence behavior through `db::Backend::storageCategory()`:
  - `Persistent`: uses on-disk storage.
  - `Ephemeral`: keeps data in-memory only (currently `memory`; useful for testing or
    ephemeral runs, not suitable for normal user data).
- Compaction is backend-capability-driven (`Backend::supportsCompaction()`):
  enabled for `lmdb` when present, skipped for `memory`.
- Cross-backend data copy used by compaction is implemented in `src/db/Maintenance.cpp` (`db::compact(...)`).
- `src/db/Maintenance.*` is reserved for backend maintenance helpers (capability checks/compaction),
  while Matrix migration orchestration belongs to `src/cache/lifecycle/`.
- Selection is delegated to `src/db/Factory.cpp`
  (`db::createDefaultBackend(...)` and `db::createConfiguredBackend(...)`),
  while backend implementations live under `src/db/`.
- Logical DB names and sync-state keys are centralized in `src/db/Catalog.cpp`
  (`db::catalog::*`) so callers don't hardcode backend-facing names
  (including legacy migration name patterns, sync-state secret key names,
  and composite value codecs such as OLM session and state-event index values).
- Reusable event-order entry parsing (including legacy raw event-id fallback) is
  centralized in `src/db/OrderEntry.cpp` (`db::parseOrderEntry(...)`).
- Reusable state-index query helpers are centralized in `src/db/StateIndex.cpp`
  (`db::findStateEventId(...)`, `db::listStateEventIds(...)`, `db::putStateEventId(...)`,
  `db::removeStateEventId(...)`) so callers do not need
  to encode backend cursor/dupsort iteration details.
- Reusable sync-state helpers are centralized in `src/db/SyncState.cpp`
  (`db::getSyncStateValue(...)`, `db::putSyncStateValue(...)`,
  `db::removeSyncStateValue(...)`, `db::getSyncStateSecretValue(...)`,
  `db::putSyncStateSecretValue(...)`, `db::removeSyncStateSecretValue(...)`)
  so callers do not manually construct sync-state and sync-state secret key names.
- Reusable OLM-session index helpers are centralized in
  `src/db/OlmSessionIndex.cpp`
  (`db::getOlmSessionValue(...)`, `db::putOlmSessionValue(...)`,
  `db::forEachOlmSessionForCurve(...)`, `db::listOlmSessionIds(...)`) so callers
  do not duplicate composite key/prefix handling for unified OLM session storage.
- Reusable Megolm index helpers are centralized in `src/db/MegolmIndex.cpp`
  (`db::megolmSessionKey(...)`, `db::parseMegolmSessionKey(...)`,
  `db::getInboundMegolmSessionValue(...)`, `db::putInboundMegolmSessionValue(...)`,
  `db::getMegolmSessionDataValue(...)`, `db::putMegolmSessionDataValue(...)`) so
  callers do not repeat JSON key encoding/parsing and twin-index access patterns
  for inbound/session-data Megolm storage.
- Reusable read-receipt key helpers are centralized in
  `src/db/ReadReceiptIndex.cpp`
  (`db::readReceiptKey(...)`, `db::getReadReceiptValue(...)`,
  `db::putReadReceiptValue(...)`) so callers do not duplicate event+room key
  encoding for read receipt payload storage.
- Reusable room-info helpers are centralized in `src/db/RoomInfo.cpp`
  (`db::serializeRoomInfo(...)`, `db::parseRoomInfo(...)`,
  `db::getRoomInfo(...)`, `db::putRoomInfo(...)`) so callers do not duplicate
  `RoomInfo` JSON serialization/parsing and keyed reads/writes in `rooms`/`invites`.
- Reusable member-info helpers are centralized in `src/db/MemberInfo.cpp`
  (`db::serializeMemberInfo(...)`, `db::parseMemberInfo(...)`,
  `db::getMemberInfo(...)`, `db::putMemberInfo(...)`) so callers do not
  duplicate `MemberInfo` JSON serialization/parsing and keyed reads/writes.
- Reusable cache-crypto struct codecs are centralized in
  `src/cache/crypto/CacheCryptoStructs.cpp`, keeping crypto/key/cache value serialization in one
  place instead of inline in `src/cache/core/Cache.cpp` so callers only parse/store values through
  typed helpers.
- Reusable generic JSON helpers are centralized in `src/db/Json.h` (`db::getJsonValue`,
  `db::putJsonValue`) so callers work with typed payloads without repeating
  parse/dump boilerplate.
- Reusable dupsort key->values iteration is centralized in `src/db/DupIndex.cpp`
  (`db::listDupValues(...)`, `db::forEachDupValue(...)`,
  `db::putDupValueForKeys(...)`, `db::replaceDupValueForKeys(...)`) to avoid repeating cursor
  `Set`/`FirstDup`/`NextDup` loops.
- Reusable key scanning is centralized in `src/db/Scan.cpp`
  (`db::listKeys(...)`, `db::listUniqueKeys(...)`, `db::listEntries(...)`
  including paged iteration, and `db::forEachEntry(...)` / `db::forEachUniqueKey(...)`
  including paged iteration where applicable, mutation helper
  `db::eraseEntriesIf(...)` (including paged erase), plus ordered-entry helpers
  such as `db::firstEntry(...)`, `db::lastEntry(...)`, and
  `db::forEachEntryFromKey(...)` / `db::forEachEntryWithPrefix(...)`) so callers can iterate DB keys/entries
  without cursor boilerplate.
- Cursor-level APIs are intentionally mostly confined to backend internals and
  helper modules (`Scan`, `DupIndex`) so higher-level code can stay backend-neutral.
- `src/cache/core/Cache.cpp` should use these helper modules instead of direct cursor operations.
- Cache DB open options (integer-key / dupsort / comparator) are centralized in
  `src/db/NamePolicy.cpp` (`db::openOptionsForName(...)`,
  `db::openOptionsForGlobal(...)`, `db::openOptionsForRoom(...)`) and consumed via
  `src/db/storage/Open.h` (`db::storage::openNamedStore(...)`,
  `db::storage::openGlobalStore(...)`, `db::storage::openRoomStore(...)`) with
  a unified `db::StoreOpenOptions` interface.

## Module Boundaries

- `src/db` is the low-level storage layer and must not depend on `src/cache` or `src/store`.
- `src/cache` (`MatrixStore`) is Matrix-domain persistence on top of `src/db`.
- Future non-Matrix persisted state should live in `src/store` (scaffolded via `src/store/README.md`)
  and use `src/db` instead of broadening `MatrixStore` semantics.

### Public Storage API Headers

- Focused storage API entry points live under `src/db/storage/`:
  - `Core.h`: backend lifecycle, IDs/capabilities, transaction helpers, core storage types
  - `Open.h`: open-options policy and store open helpers
  - `Scan.h`: key/value scan and dupsort iteration helpers
  - `State.h`: state-event index helpers
  - `SyncState.h`: sync-state typed helper surface
  - `Crypto.h`: OLM/Megolm/read-receipt helper surface
  - `Serde.h`: typed JSON/value helpers (`Json`, `toSv`)

### Migration Note

- New callsites should prefer focused includes, for example:
  - `#include "db/storage/Core.h"` + `#include "db/storage/Open.h"` for lifecycle/store-open code
  - `#include "db/storage/Scan.h"` for entry iteration utilities
  - `#include "db/storage/SyncState.h"` for sync-state utilities
- Cache code should use `src/cache/schema/CacheSchema.h` for Matrix schema mapping/wrappers
  instead of using `db::catalog::*` directly.
- Cache code should use `src/cache/schema/Codecs.h` (`cache::codec::*`) for `RoomInfo`/`MemberInfo`
  serialization helpers instead of importing those domain codecs from `src/db/storage/Serde.h`.

## Prefixes

Filesystem prefixes:

- settings root: `~/.config/komai/profiles/`
- data root: `~/.local/share/komai/profiles/`
- cache root: `~/.cache/komai/profiles/`

Secret-store key prefixes.
`<env-tag>` isolates secrets across packaging formats depending on the filesystem
config paths they use, so that each unique filesystem path produces a unique keyring prefix
(`native`, `flatpak`, `snap`, or a 6-char hex hash):

- `komai.<env-tag>.<profile-id>.settings.`
- `komai.<env-tag>.<profile-id>.local_crypto.`
- `komai.<env-tag>.<profile-id>.matrix.`

## Main Call Sites

- `src/UserSettingsPage.cpp` (profile YAML files)
- `src/cache/setup/CacheSetup.cpp` (database base directory)
- `src/ui/ThemeRegistry.cpp` (external theme search directories)
- `src/MatrixClient.cpp` (curl alt-svc cache file)
- `src/MxcImageProvider.cpp` (media purge + media cache file paths)
- `src/ui/MxcAnimatedImage.cpp` (media cache file paths)
- `src/ui/MxcMediaProxy.cpp` (media cache file paths)
- `src/timeline/TimelineModel.cpp` (downloaded media cache paths)
- `src/main.cpp` (cache/data directory creation and log file path)
- `src/notifications/ManagerWin.cpp` (cached room avatar path)
