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
- Selection is delegated to `src/db/Factory.cpp`
  (`db::createDefaultBackend(...)` and `db::createConfiguredBackend(...)`),
  while backend implementations live under `src/db/`.
- Logical DB names and sync-state keys are centralized in `src/db/Catalog.cpp`
  (`db::catalog::*`) so callers don't hardcode backend-facing names
  (including legacy migration name patterns, sync-state secret key names,
  and composite value codecs such as OLM session and state-event index values).
- Reusable schema/migration primitives (for example, full-resync room DB lists,
  drop helpers, and legacy migration helpers) are centralized in
  `src/db/Schema.cpp` (`db::roomDbsForFullResync(...)`,
  `db::tryDropNamedStore(...)`, `db::migrateLegacyStateByKeyToStatesKey(...)`,
  `db::migrateLegacyMegolmSessionIndexes(...)`, `db::migrateLegacyOlmShardsV1ToV2(...)`,
  `db::migrateLegacyOlmShardsV2ToUnified(...)`).
- Reusable event-order entry parsing (including legacy raw event-id fallback) is
  centralized in `src/db/OrderEntry.cpp` (`db::parseOrderEntry(...)`).
- Reusable timeline-index helpers are centralized in `src/db/TimelineIndex.cpp`
  (`db::removeMessageOrderMapping(...)`,
  `db::replaceTimelineEventId(...)`,
  `db::putEventOrderMapping(...)`,
  `db::putOrderEntry(...)`,
  `db::putEventOrderMappingForEvent(...)`,
  `db::putMessageOrderMapping(...)`,
  `db::appendEventOrderEntry(...)`,
  `db::prependEventOrderEntry(...)`,
  `db::appendMessageOrderEntry(...)`,
  `db::prependMessageOrderEntry(...)`,
  `db::lastInvisibleEventAfter(...)`,
  `db::lastVisibleEvent(...)`,
  `db::lastTimelineEventId(...)`,
  `db::timelineRange(...)`,
  `db::timelineIndexForEvent(...)`,
  `db::eventIndexForEvent(...)`,
  `db::timelineEventIdAtIndex(...)`,
  `db::firstOrderedIndex(...)`,
  `db::lastOrderedIndex(...)`,
  `db::firstPrevBatchToken(...)`,
  `db::setOrderEntryPrevBatch(...)`,
  `db::removePendingEntriesByTxnId(...)`,
  `db::listOrderEntriesAfterPrevBatchMarker(...)`,
  `db::listOrderEntryEventIds(...)`,
  `db::removeMessageOrderMappingsNotInOrderEntries(...)`,
  `db::removeOrderEntryReferences(...)`,
  `db::removeOrderEntryWithReferences(...)`,
  `db::eraseOrderEntriesWithReferencesIf(...)`,
  `db::trimOldestOrderEntriesWithReferences(...)`,
  `db::cleanupTimelineBeforePrevBatchMarker(...)`,
  `db::removeTimelineEventReferences(...)`) so callers do not duplicate
  cross-index logic (`message_to_order`, `order_to_message`,
  `event_to_order`, event payload/relation deletion, and visible/invisible
  timeline lookups).
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
  `src/db/StorageApi.h` (`db::storage::openNamedStore(...)`,
  `db::storage::openGlobalStore(...)`, `db::storage::openRoomStore(...)`) with
  a unified `db::StoreOpenOptions` interface.

## Prefixes

Filesystem prefixes:

- settings root: `~/.config/komai/profiles/`
- data root: `~/.local/share/komai/profiles/`
- cache root: `~/.cache/komai/profiles/`

Secret-store key prefixes:

- `komai.<profile-id>.settings.`
- `komai.<profile-id>.local_crypto.`
- `komai.<profile-id>.matrix.`

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
