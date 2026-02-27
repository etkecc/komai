# Cache Architecture

This module stores Matrix client state on top of the generic `src/db` storage layer.

## Responsibilities

- Persist Matrix sync state, room metadata, membership, timeline indices/events, account data.
- Persist encryption-related local state (OLM/Megolm sessions and verification-related cache data).
- Provide app-facing cache APIs and lifecycle orchestration.

## Non-Responsibilities

- Generic key-value backend mechanics (transactions, cursor semantics, backend capabilities).
- Backend-specific implementation details (LMDB internals, map sizing behavior, low-level schema helpers).

Those belong to `src/db`.

## Module Layout (`src/cache/`)

- `Cache.h` - public module entry header for application-facing cache APIs.
- `api/` - wrapper API declarations and externally-facing cache entry points.
- `core/` - core store implementation and shared helpers (`core/Cache.h` is a compatibility shim).
- `setup/` - cache startup/open/compaction/setup wiring.
- `lifecycle/` - migration and sync/lifecycle orchestration.
- `timeline/` - timeline persistence/index/read/pending flows.
- `rooms/` - room info and room metadata helpers.
- `membership/` - member list/invite/membership persistence helpers.
- `spaces/` - spaces and image-pack persistence helpers.
- `crypto/` - crypto/session persistence helpers and codecs.
- `users/` - user profiles/keys/trust helpers.

## Related Docs

- [Storage Architecture](../storage.md)
- [Path Migration Map](migration-map.md)
