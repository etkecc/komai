# Cache Architecture

This module stores Matrix client state on top of the generic `src/db` storage layer.

## Responsibilities

- Persist Matrix sync state, room metadata, membership, timeline indices/events, account data.
- Persist encryption-related local state (OLM/Megolm sessions and verification-related cache data).
- Provide app-facing cache APIs and lifecycle orchestration.
- Keep Matrix-domain persistence scoped to `MatrixStore`.

## Non-Responsibilities

- Generic key-value backend mechanics (transactions, cursor semantics, backend capabilities).
- Backend-specific implementation details (LMDB internals, map sizing behavior, low-level schema helpers).

Those belong to `src/db`.

## Naming Scope

`MatrixStore` is intentionally Matrix-specific. If Komai needs to persist unrelated app data later,
add a separate module/store on top of `src/db` instead of broadening `MatrixStore` semantics.

## Migration Policy

- Cache format compatibility is governed by a single cache-format version key in sync-state.
- Komai no longer carries a long legacy migration chain from pre-fork historical formats.
- On incompatible/older cache formats, Komai resets cache DB content and continues with the
  current format baseline.
- Future migrations should be added only for Komai-owned format transitions that must preserve
  local state across releases.

## Module Layout (`src/cache/`)

- `Cache.h` - public module entry header for application-facing cache APIs.
- `api/` - wrapper API declarations and externally-facing cache entry points.
- `core/` - core store implementation and shared helpers.
- `setup/` - cache startup/open/compaction/setup wiring.
- `lifecycle/` - migration and sync/lifecycle orchestration.
- `timeline/` - timeline persistence/index/read/pending flows.
- `rooms/` - room info and room metadata helpers.
- `membership/` - member list/invite/membership persistence helpers.
- `spaces/` - spaces and image-pack persistence helpers.
- `crypto/` - crypto/session persistence helpers and codecs.
- `users/` - user profiles/keys/trust helpers.

## API Unit Map (`src/cache/api/*.cpp`)

- `CacheApiInstance.cpp` - cache instance lifecycle and availability checks.
- `CacheApiLifecycleOps.cpp` - setup/sync/migration and cache lifecycle wrappers.
- `CacheApiUsers.cpp` - user/presence/key/verification wrapper functions.
- `CacheApiRoomsInfo.cpp` - room metadata/accessor wrappers.
- `CacheApiRooms.cpp` - room state/member/read-status related wrappers.
- `CacheApiTimeline.cpp` - timeline/event/pending/notification wrappers.
- `CacheApiCrypto.cpp` - Olm/Megolm/secrets/backup wrapper functions.
- `CacheApiSignals.cpp` - Qt signal connection helpers for cache events.
- `CacheApiStateTemplates.cpp` - explicit state-event template instantiations for `MatrixStore`.
- `CacheApiLoggers.cpp` - cache logger wiring and defaults.

## Related Docs

- [Storage Architecture](../storage.md)
- [Path Migration Map](migration-map.md)
