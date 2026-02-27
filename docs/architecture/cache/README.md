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

- `api/` - compatibility wrappers and externally-facing cache entry points.
- `core/` - core cache class/state and shared helpers.
- `setup/` - cache startup/open/compaction/setup wiring.
- `lifecycle/` - migration and sync/lifecycle orchestration.
- `timeline/` - timeline persistence/index/read/pending flows.
- `rooms/` - room info and room metadata helpers.
- `membership/` - member list/invite/membership persistence helpers.
- `spaces/` - spaces and image-pack persistence helpers.
- `crypto/` - crypto/session persistence helpers and codecs.
- `users/` - user profiles/keys/trust helpers.

## Transitional Compatibility

Legacy include paths at `src/Cache*.h` are currently kept as thin forwarding headers during migration. New code should include headers from `src/cache/*`.

## Related Docs

- [Storage Architecture](../storage.md)
