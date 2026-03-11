# Cache Storage Invariants

This document records the current on-disk cache rules that other cache changes should preserve.

## Compatibility and Reset Policy

- Cache compatibility is controlled by one cache-format version stored in sync-state.
- Compatibility is exact-match only. Older and newer cache formats are both treated as incompatible.
- On incompatible cache formats, Komai resets rebuildable cache content instead of carrying a long migration chain.
- Settings/profile files are separate from the cache and are not part of this reset contract.

## LMDB Headroom

- The default `maxStores` target is `131072`.
- This is intentional headroom for large accounts and room churn under the current named-store layout.
- Higher headroom is a guardrail, not the primary cleanup strategy. Room removal must still free room-owned storage.

## Room Cleanup

- Leaving or removing a room is an authoritative cache cleanup action.
- Room-scoped named stores are dropped by room prefix, not selectively cleared one table at a time.
- Room-keyed global cache state is also removed, including:
  - room/invite metadata
  - derived space edges
  - current read receipts for that room
  - room-scoped encryption/session cache data
  - sent-notification markers for events in that room
- Compaction should never be relied on as orphan cleanup. The live teardown path must remove the data first.

## Read Receipt Semantics

- The source of truth is one current read receipt per `(room, user)`.
- Stored receipt state includes the current event id, timestamp, and cached event-order index.
- Event-based queries are derived from that current state and answer:
  - who has currently read up to this event?
- Komai does not preserve accidental historical residue of where a receipt used to be before it advanced.

## Timeline Relation Cleanup

- Relation indices must be updated as current state, not as append-only history.
- Removing a timeline event must delete relation entries where that event was either:
  - the target key
  - the related/source event id stored under another key
- Event replacement and txn-id remapping must rewrite source-side relation references instead of only appending new ones.

## Derived Space Graph

- `spacesChildren` and `spacesParents` are derived indices, not independent source-of-truth data.
- The derived graph is the union of:
  - valid `m.space.child` state
  - valid authorized `m.space.parent` state
- Parent-side refresh must remove only stale indexed edges and keep child-derived edges intact.
- Live updates, full-state refreshes, room removal, and redactions of relevant space state must all keep the derived graph authoritative.

## Maintenance UI Implications

- A future cache-maintenance UI can safely expose cache-size reporting and destructive cleanup actions because incompatible-cache resets are already part of the design.
- LMDB compaction currently follows an offline copy/swap/reopen model, so a future UI action will likely require a restart-aware flow rather than in-place live compaction.
