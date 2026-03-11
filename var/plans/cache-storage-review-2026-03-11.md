# Cache Storage Review Notes

Date: 2026-03-11

Status: in progress

Scope:
- Review Komai's persisted cache/storage layout before first release.
- Focus on breaking changes that are still cheap now.
- Prioritize schema/layout problems, cleanup gaps, and LMDB scaling risks.

## Decision Log

- [x] Leave cleanup policy: aggressive.
  - After a room is left/removed, clean up as much room-owned local cache state as practical.
- [x] Project scope: full rewrite now, not staged delivery.
  - The goal is to put storage on solid footing before launch, not to ship incremental mitigation first.
- [x] Old cache compatibility: hard reset is acceptable.
  - Prefer simple reset/rebuild over adding migration code.
  - If possible, rely on the existing incompatible-cache reset path so beta testers do not need to wipe full profiles manually.
  - If that stops being clean/simple, manual fresh-start instructions are acceptable.
- [x] Default `maxStores` target: `131072`.
- [x] Read-receipt UI semantics: "read up to here".
  - Storage direction is clear: stop using append-only per-event blobs as the source of truth.
  - For an older message, the UI should answer "whose current receipt is at or after this message?"
  - We do not need to preserve accidental historical residue of where a receipt used to be long ago.

## Task Board

Investigation / planning:
- [x] Map the cache schema and named-store layout.
- [x] Review cache startup, format handling, compaction, and cleanup paths.
- [x] Review timeline persistence, relations, room removal, read receipts, and spaces.
- [x] Review the settings persistence layer for must-break-now issues.
- [x] Run the local test suite on the current tree.
- [x] Quantify the `maxStores` pressure with rough room-store math.
- [x] Identify missing cache-level tests that would have caught the current issues.
- [x] Write and maintain this working note in `var/plans`.

Concrete implementation work:
- [x] Fix the first-launch-after-cache-reset path so it does not keep stale LMDB handles or missing-store reads after an incompatible-format reset.
- [x] Fix room teardown so it drops all room-local stores, not just `rooms/state/account_data/members`.
- [x] Implement the aggressive cleanup policy for room-keyed global state on room leave/removal.
- [x] Redesign read receipt storage around current-state semantics.
- [x] Raise the default `maxStores` headroom to `131072`.
- [x] Make cache compatibility exact-match only.
- [x] Remove dead cache-schema baggage (`PendingReceipts`, any no-longer-needed legacy shims).
- [x] Build a small MatrixStore-focused integration test harness so cache lifecycle/reset behavior can be tested without manual launches.
- [x] Add focused MatrixStore integration tests for incompatible-reset reuse, room teardown, and advancing read receipts.
- [x] Extend MatrixStore integration coverage to limited-sync cleanup behavior.
- [x] Add MatrixStore integration coverage for derived space-edge rebuild / reparenting behavior.
- [x] Fix relation cleanup so stale downstream IDs do not survive.
- [x] Fix additive room-parent space cleanup so parent unlinks and full-state refreshes stop leaving stale edges behind.
- [x] Rebuild derived space edges when redacted parent/child state removes raw state without a live replacement event.
- [x] Replace most per-room named DBs with a fixed set of shared room stores keyed by `room_id + subkey`.
- [x] Decide the shared-store grouping/key-encoding plan so ordered scans and dupsort semantics stay efficient.
- [x] Start the shared-room-store rewrite with the non-timeline room stores.
- [ ] Design cache maintenance / statistics UI after the storage rewrite settles.

Documentation follow-up:
- [x] Update `docs/user-guide/differences-from-nheko.md` with a short user-facing summary of the storage/cache behavior changes that matter at launch.
- [x] Add a `docs/architecture/` note that documents the final cache-storage decisions (for example read-receipt policy, cache reset rules, and room-cleanup invariants).

## Current State

- The main persisted app data is LMDB-backed, not a relational DB.
- Settings persistence is separate YAML (`config.yml`, `state.yml`, `session.yml`) with schema versioning.
- Cache compatibility is controlled by a single format version string in `src/cache/lifecycle/CacheLifecycleMigrations.cpp`.
- On incompatible cache format, Komai resets almost the entire cache DB instead of carrying a long migration chain.

## Progress So Far

- Mapped the cache schema and store layout.
- Reviewed cache startup, format versioning, compaction, and cleanup code.
- Reviewed timeline persistence, relations, room removal, read receipts, spaces, and settings migrations.
- Narrowed the room-store footprint more precisely from the active sync paths.
- Confirmed that backend `drop(..., true)` semantics are intended to delete named stores, not just clear contents.
- Ran the local test suite to make sure the current tree is green while reviewing.
- Committed foundation storage changes:
  - default `maxStores` now targets `131072`
  - cache format compatibility is exact-match only
- Committed authoritative room cleanup changes:
  - room removal now drops all room-local named stores by room-prefix
  - room removal now deletes room-keyed global cache residue (`rooms`, `invites`, spaces edges, read receipts, encrypted-room flags, expiry-job state, outbound Megolm state, inbound/session metadata, and sent-notification markers for room events)
  - public remove-room call sites now rely on the single authoritative cleanup path instead of chaining `removeInvite()` afterward
- Committed read-receipt storage changes:
  - cache format bumped again so the new receipt store shape resets cleanly
  - read receipts now store one current receipt per `(room, user)` instead of append-only per-event blobs
  - receipt queries now implement the chosen "read up to here" semantics
  - timeline/read-receipt UI paths now refresh from current cache state instead of keeping stale receipt markers forever
- Added helper-level DB coverage for the new room-scoped read-receipt key/index helpers.
- Added a dedicated MatrixStore integration harness:
  - new `komai_matrix_store_test` target
  - test-only stubs for non-storage runtime dependencies so the harness can instantiate `MatrixStore` without manual app launches
  - coverage for first-launch incompatible-reset reuse, authoritative room teardown, and advancing read receipts under the chosen "read up to here" semantics
- Extended MatrixStore integration coverage further:
  - limited sync now has integration coverage for reclaiming stale timeline payloads, relations, and pending entries while preserving event payloads still referenced by current state
  - derived space edges now have integration coverage for room wipes, child-derived edge preservation, authorized reparenting, and stale-edge removal after parent-space refreshes
- Tightened the limited-sync cleanup path itself:
  - limited sync now clears stale relation rows and old pending entries as part of the reset
  - stale timeline event payloads are deleted, but event payloads still referenced by current room state are preserved
- Committed dead-baggage cleanup:
  - removed the unused `PendingReceipts` schema/catalog entry
  - removed the old raw-string order-entry compatibility fallback, since pre-release cache resets are already intentional
- Prepared and verified relation-cleanup changes:
  - timeline event cleanup now removes relation entries where the deleted event was the source/related event, not only where it was the target key
  - timeline write paths now rewrite relation source references on duplicate-event updates and txn-id remaps instead of only appending new dupsort values
  - helper-level DB coverage now checks source-side relation cleanup for direct deletes, order-entry cleanup, trimming, and prev-batch-marker cleanup
- Prepared and verified space-edge cleanup changes:
  - room-parent edge refresh now removes only stale indexed parent links before re-adding current valid ones
  - the cleanup preserves the intended union semantics between `m.space.child` and `m.space.parent` instead of bluntly dropping child-derived edges
  - explicit parent unlinks and full-state refreshes no longer leave stale `room -> space` / `space -> room` cache edges behind
- Prepared and verified space-redaction cleanup changes:
  - redactions of cached `m.space.child` and `m.space.parent` state now schedule a derived space-edge rebuild immediately
  - the derived graph no longer depends on seeing a live replacement event in the same sync batch before it can remove stale edges
- Added cache-storage documentation:
  - `docs/architecture/cache/storage-invariants.md` now records the cache reset policy, room cleanup contract, read-receipt semantics, relation cleanup rules, and derived space-graph invariants
  - `docs/user-guide/differences-from-nheko.md` now includes a short user-facing note about the more resilient local cache behavior
- Prepared and verified the incompatible-cache reset fix:
  - after a format reset, MatrixStore now recreates/reopens its core named stores before restore code runs
  - this avoids using dropped LMDB DB handles and avoids read-only opens of missing stores on the first launch after reset
  - this bug was observed in a real manual run where the first launch after reset dropped to login and the second launch succeeded
- Committed shared room-store chunk 1:
  - commit: `76f06ee9d` (`cache: share room state and membership stores`)
  - non-timeline room-local stores now live in the fixed shared stores:
    - `state`
    - `states_key`
    - `members`
    - `invite_state`
    - `invite_members`
    - `account_data`
- Prepared and verified shared room-store chunk 2:
  - room-local timeline stores now also live in the fixed shared stores:
    - `events`
    - `event_order`
    - `event2order`
    - `msg2order`
    - `order2msg`
    - `pending`
    - `related`
  - added a cache-layer room-scoped timeline helper layer so the db module stays room-agnostic while shared stores still preserve:
    - ordered scans by event/message index
    - txn-id remaps
    - relation rewrites / cleanup
    - limited-sync trimming and prev-batch cleanup
  - authoritative room cleanup now erases all shared timeline rows and room-owned notifications
  - read-receipt index lookups now read `event2order` through shared composite keys
  - shared ordered stores no longer rely on LMDB `Append` puts:
    - the old hint was safe for room-local integer-key DBs
    - it is not globally safe once multiple rooms share one ordered store
    - the shared rewrite keeps the cursor semantics but writes ordered rows with normal puts

## Checks Run

- Targeted persistence-related tests:
  - `ctest --test-dir var/build/native --output-on-failure -R 'komai_(yaml_settings_test|staged_load_plan_test|startup_settings_test|settings_storage_test|settings_store_test|setting_input_validation_test|settings_serializer_core_test|db_backend_test)'`
  - Result: passed
- Full local CTest suite:
  - `ctest --test-dir var/build/native --output-on-failure`
  - Result: 18/18 passed
- After the foundation storage patch:
  - `just build`
  - `just test`
  - Result: passed
- After the authoritative room-cleanup patch:
  - `just build`
  - `just test`
  - Result: passed
- After the read-receipt storage patch:
  - `just build`
  - `just test`
  - Result: passed
- After the dead-baggage cleanup patch:
  - `just build`
  - `just test`
  - Result: passed
- After the relation-cleanup patch:
  - `just build`
  - `just test`
  - Result: passed
- After the room-parent space cleanup patch:
  - `just build`
  - `just test`
  - Result: passed
- After the space-redaction cleanup patch:
  - `just build`
  - `just test`
  - Result: passed
- After the documentation patch:
  - `just lint`
  - Result: passed
- After the incompatible-cache reset-path fix:
  - `just build`
  - `just test`
  - Result: passed
- After the MatrixStore integration-harness patch:
  - `just build`
  - `ctest --output-on-failure -R komai_matrix_store_test`
  - `just test`
  - `just lint`
  - Result: passed
- After the limited-sync / space-edge MatrixStore coverage patch:
  - `just build`
  - `ctest --output-on-failure -R komai_matrix_store_test`
  - `just test`
  - Result: passed
- After shared room-store chunk 1:
  - `just build`
  - `ctest --test-dir var/build/native --output-on-failure -R 'komai_(db_backend_test|matrix_store_test)'`
  - `just test`
  - Result: passed
- After shared room-store chunk 2:
  - `just build`
  - `ctest --test-dir var/build/native --output-on-failure -R 'komai_(db_backend_test|matrix_store_test)'`
  - `just test`
  - `just lint`
  - Result: passed

## Test Coverage Gaps Observed

- There are helper-level DB tests for read-receipt key encoding and dupsort behavior.
- Helper-level DB coverage now also checks relation cleanup on:
  - direct event-reference deletion
  - relation target rewrites
  - order-entry cleanup
  - trim-oldest timeline cleanup
  - clear-before-prev-batch-marker cleanup
- There is now a MatrixStore-level integration harness that exercises:
  - incompatible-format reset followed by same-process store reuse
  - room teardown removing room-local stores and room-keyed global cache residue
  - advancing read receipts from one event to another with "read up to here" queries
- limited timeline sync reclaiming old event payloads and relations is now covered at the MatrixStore integration level
- derived space-edge rebuild / reparenting cleanup is now covered at the MatrixStore integration level

Implication:
- The green suite now protects the main reset, teardown, read-receipt, limited-sync cleanup, and derived-space-edge paths that drove this storage review.
- The next meaningful coverage work should follow the shared-store rewrite rather than trying to grow the current layout-specific test surface much further.

## Next Implementation Track

If continuing this work after a restart, the next preferred order is:

1. Reassess `maxStores` pressure and cleanup complexity now that the shared-store rewrite is in place.
2. Decide whether any extra rewrite-specific coverage is still missing after living with the new layout for a bit.
3. Then start the cache stats / maintenance UI work.

## Restart Handoff

If this work gets restarted in a fresh session, pick it up in this order:

1. Reassess `maxStores` pressure and cleanup complexity on top of the shared-store layout.
2. Add any extra rewrite-specific coverage only if a concrete gap appears.
3. Then move on to cache maintenance / statistics UX work.

Current code checkpoint:
- Latest storage/integration commit before the shared-store rewrite: `3658fe86a` (`Cover limited sync and space cache integration paths`)
- Shared-store chunk 1 commit: `76f06ee9d` (`cache: share room state and membership stores`)
- Earlier harness foundation commit: `801495c20` (`Add MatrixStore cache integration tests`)
- The shared-store rewrite is green with:
  - `just build`
  - `ctest --test-dir var/build/native --output-on-failure -R 'komai_(db_backend_test|matrix_store_test)'`
  - `just test`
  - `just lint`

Key files for the next steps:
- `var/plans/cache-storage-review-2026-03-11.md`
- `src/cache/schema/RoomStore.h`
- `src/cache/schema/RoomTimelineIndex.h`
- `src/cache/lifecycle/CacheLifecycleRooms.cpp`
- `tests/MatrixStoreIntegrationTest.cpp`

What the next session should remember:
- The harness uses `ScopedTestHome`, so it writes to a temporary XDG tree under `/tmp`, not the real profile directories.
- The current MatrixStore integration coverage already protects:
  - incompatible-format reset followed by same-process store reuse
  - authoritative room teardown
  - advancing read receipts under the chosen "read up to here" semantics
  - limited sync clearing stale timeline indexes / relations without deleting still-live state payloads
  - derived space-edge rebuild / reparenting behavior across room and space refreshes
- The structural shared-store rewrite is now in place; the next work is post-rewrite assessment rather than another schema rewrite.
- Shared-room-store rewrite status:
  - chunk 1 committed:
    - `state`
    - `states_key`
    - `members`
    - `invite_state`
    - `invite_members`
    - `account_data`
  - chunk 2 implemented and green:
    - `events`
    - `event_order`
    - `event2order`
    - `msg2order`
    - `order2msg`
    - `pending`
    - `related`
  - current format string: `2026.03.11.2`
  - the shared ordered stores intentionally do not use LMDB `Append` writes anymore

## Shared Room Store Rewrite Plan

Concrete grouping to implement:

- `shared_room_plain`
  - Plain key/value store.
  - Logical room stores keyed as `logical_store + '\0' + room_id + '\0' + subkey`.
  - Intended contents:
    - `state`
    - `members`
    - `invite_state`
    - `invite_members`
    - `account_data`
    - later: `events`, `event2order`, `msg2order`
- `shared_room_ordered`
  - Plain key/value store with lexicographically ordered composite keys.
  - Logical room stores keyed as `logical_store + '\0' + room_id + '\0' + be64(order_index)`.
  - Intended contents:
    - `event_order`
    - `order2msg`
    - `pending`
- `shared_room_dupsort`
  - Dupsort store.
  - Logical room stores keyed as `logical_store + '\0' + room_id + '\0' + subkey`.
  - Intended contents:
    - `states_key`
    - later: `related`
  - The existing `StateKey` dupsort comparator remains usable because `states_key` values keep the
    current `state_key + '\0' + event_id` layout, while `related` values remain plain event IDs and
    therefore compare as whole strings.

Key implementation notes:

- Use NUL-separated composite keys; Matrix room IDs / event IDs / user IDs / event types do not
  contain NUL bytes, so the encoding is unambiguous and cheap to parse.
- Use fixed-width big-endian order encodings in `shared_room_ordered` so prefix scans preserve the
  current numeric timeline / pending ordering without per-room integer-key LMDB DBs.
- Keep cleanup authoritative at the room level:
  - remove-room must erase every room-owned entry from all shared room stores
  - no migration code will be added; cache-format reset remains the compatibility strategy

Preferred implementation order:

1. Move non-timeline room stores first:
   - `state`
   - `states_key`
   - `members`
   - `invite_state`
   - `invite_members`
   - `account_data`
2. Then move timeline payloads / indexes:
   - `events`
   - `event_order`
   - `event2order`
   - `msg2order`
   - `order2msg`
   - `pending`
   - `related`
3. After both chunks land, reassess residual `maxStores` pressure, cleanup complexity, and any
   rewrite-specific test coverage gaps.

Chunk 1 details:

- Added three fixed shared room stores to the schema/catalog:
  - `shared_room_plain`
  - `shared_room_ordered`
  - `shared_room_dupsort`
- Moved the non-timeline room-local stores onto shared composite-key storage:
  - `state`
  - `states_key`
  - `members`
  - `invite_state`
  - `invite_members`
  - `account_data`
- Added reusable room-key helpers for:
  - NUL-separated `logical_store + room_id + subkey` keys
  - room-prefix scans / counts / deletes
  - shared ordered-key encoding helpers for the next chunk
- Replaced room-wide `drop()` calls that would be unsafe on shared stores:
  - `removeInvite()`
  - `removeRoom()`
  - `updateState(..., wipe = true)`
- Bumped the cache format again to force a hard reset onto the new mixed old/new layout:
  - current format string: `2026.03.11.1`
- Added coverage for the new chunk-1 invariants:
  - `DbBackendTest` now checks the shared dupsort store name policy
  - `MatrixStoreIntegrationTest` now verifies that `removeRoom()` deletes shared room-scoped
    `state` / `states_key` / `members` / invite / account-data rows while preserving global
    account-data entries

Chunk 1 checks:

- `just build`
  - Result: passed
- `ctest --test-dir var/build/native --output-on-failure -R 'komai_(db_backend_test|matrix_store_test)'`
  - Result: passed
- `just test`
  - Result: passed

Chunk 2 details:

- Moved the timeline room-local stores onto shared composite-key storage:
  - `events`
  - `event_order`
  - `event2order`
  - `msg2order`
  - `order2msg`
  - `pending`
  - `related`
- Added room-scoped shared timeline helpers for:
  - ordered first/last/range scans by room prefix
  - event/message index lookups
  - txn-id remaps
  - pending cleanup
  - relation-source rewrites and cleanup
  - limited-sync / prev-batch cleanup paths
- Switched the remaining cache paths that still depended on raw room-local event keys:
  - timeline read/write/pending paths
  - room cleanup / notification cleanup
  - read-receipt event-index lookups
  - state-event payload reads/writes through the shared `events` store
- Bumped the cache format again to force a hard reset onto the fully shared room-store layout:
  - current format string: `2026.03.11.2`
- Added / extended integration coverage for the new chunk-2 invariants:
  - `MatrixStoreIntegrationTest` now seeds and verifies shared timeline entries in `removeRoom()`
  - limited-sync integration coverage now seeds and checks the shared timeline/event/relation/pending stores directly
- Important implementation note:
  - shared ordered stores no longer pass LMDB `Append` put flags because that optimization is not
    globally valid once multiple rooms share the same ordered DB

Chunk 2 checks:

- `just build`
  - Result: passed
- `ctest --test-dir var/build/native --output-on-failure -R 'komai_(db_backend_test|matrix_store_test)'`
  - Result: passed
- `just test`
  - Result: passed
- `just lint`
  - Result: passed

Verification commands for the next steps:
- `just build`
- `ctest --test-dir var/build/native --output-on-failure -R 'komai_(db_backend_test|matrix_store_test)'`
- `just test`
- `just lint` for docs-only or mixed patches when appropriate

## Shared-Store Rewrite Direction

Current issue:
- A joined room currently costs roughly 10-11 named LMDB stores.
- That means `maxStores` pressure scales roughly linearly with room count, even after cleanup fixes.

Target direction:
- Stop creating most room-local named stores like `!room/...`.
- Instead, keep a fixed number of top-level stores and key them by `room_id + subkey`.
- The count should be constant for the whole profile, not linear in room count.

Important constraint:
- This should probably not collapse all room data into a single store.
- Different access patterns need different store properties:
  - plain key/value payloads
  - ordered timeline indexes
  - reverse lookup indexes
  - dupsort relation/state-key indexes

Most likely outcome:
- not "11 stores per room becomes 1 total store"
- more like "11 stores per room becomes a small constant number of shared stores overall"
- exact grouping still to be designed, but the important win is removing per-room DB-slot growth

Expected benefits:
- `maxStores` pressure mostly disappears as a scaling concern
- orphaned-store risk drops sharply because room removal becomes prefix/range cleanup, not DB-handle cleanup
- cache stats / maintenance UI becomes easier because the storage layout is more centralized

Expected tradeoffs:
- exact per-room iteration becomes prefix/range scans in shared stores instead of scanning a dedicated room DB
- some current integer-key room stores will likely become lexicographically encoded composite keys, which may cost a small constant-factor performance hit
- room deletion becomes "delete this room's key range from several shared stores" instead of dropping whole named DBs

Current expectation:
- This should be a worthwhile trade even if some operations get slightly slower in constant terms
- the structural gain is that room count stops consuming LMDB DB slots linearly

## Findings

### 1. Room teardown leaks room-owned LMDB stores

Severity: high

Relevant code:
- `src/cache/schema/CacheSchema.h:39`
- `src/cache/core/CacheCoreStorage.cpp:79`
- `src/cache/lifecycle/CacheLifecycleRooms.cpp:58`
- `src/cache/lifecycle/CacheLifecycleRooms.cpp:67`
- `src/chat/ChatPageBootstrap.cpp:208`
- `src/chat/ChatPageRoomActions.cpp:164`
- `src/db/Maintenance.cpp:67`

What is happening:
- `RoomDb` defines many room-scoped stores.
- `MatrixStore::removeRoom(db::Transaction&, ...)` only drops:
  - `state`
  - `account_data`
  - `members`
- `MatrixStore::removeRoom(const std::string &)` only deletes the `rooms` row.
- Leave-room flows call the public overload, so they do not fully clean room-owned stores.
- Compaction copies every named store, so orphan stores survive compaction too.

Likely leaked room-local stores today:
- Sync-driven left-room cleanup appears to strand at least:
  - `events`
  - `event_order`
  - `event_to_order`
  - `message_to_order`
  - `order_to_message`
  - `pending`
  - `related`
  - `states_key`
- The public/UI removal path appears to strand essentially the full joined-room footprint, because it does not drop any room-local DBs at all.

Why this matters:
- Joined/left historical rooms continue to consume named DB slots.
- Users with many rooms, or who churn through rooms over time, accumulate store-count debt.
- This directly reinforces the `max_dbs` problem.

Practical implication:
- Even if `maxStores` is raised, orphaned room DBs will continue to pile up unless the teardown model changes.

### 2. `max_dbs` exhaustion is structural, not just a tuning issue

Severity: high

Relevant code:
- `src/cache/core/CacheCoreStorage.cpp:79`
- `src/cache/setup/CacheSetup.cpp:36`
- `src/cache/setup/CacheSetup.cpp:203`
- `src/cache/lifecycle/CacheLifecycleSyncPersist.cpp:266`

What is happening:
- Komai allocates many named LMDB DBs per room.
- Default `maxStores` is `32384`.
- On `DbsFull`, Komai increases the setting and exits so the user can restart.

Notes:
- Active joined-room sync appears to open these room-local stores eagerly:
  - `state`
  - `states_key`
  - `members`
  - `events`
  - `event_order`
  - `event_to_order`
  - `message_to_order`
  - `order_to_message`
  - `pending`
  - `related`
- Room account data adds `account_data` when used.
- Global account data is also currently stored via the same room-store mechanism with an empty room ID, which effectively creates a synthetic `"/account_data"` named store.
- Invite rooms appear to use:
  - `invite_state`
  - `invite_members`
- So a normal joined room is roughly a 10-11 named-store cost under the current layout.
- Rough upper-bound math with ~17 non-room stores:
  - `32384` max stores supports about `3236` rooms at 10 stores each
  - `32384` max stores supports about `2942` rooms at 11 stores each
  - `65536` max stores supports about `5956` rooms at 11 stores each
  - `131072` max stores supports about `11914` rooms at 11 stores each
- So a clean 3000-room account is already borderline under the current default, even before historical leaks are counted.
- Historical rooms make this worse if teardown is incomplete.

Conclusion:
- Raising the default is reasonable as a temporary pressure valve.
- It is not a real fix for the current store layout.

Supporting references:
- `src/cache/lifecycle/CacheLifecycleSyncPersist.cpp:65`
- `src/cache/timeline/CacheTimelineWrite.cpp:60`
- `src/cache/lifecycle/CacheLifecycleStateWrite.cpp:32`
- `src/cache/lifecycle/CacheLifecycleSyncPersist.cpp:155`
- `src/cache/lifecycle/CacheLifecycleSyncPersist.cpp:41`
- `src/db/Catalog.cpp:80`

Workarounds to consider:
- Raise default `maxStores` to `131072`.
- Add an orphan-store sweep during cache format bump / startup maintenance.
- Long-term: collapse per-room named DBs into a small number of global stores keyed by `room_id + subkey`.

Important nuance:
- Proper store deletion should help materially here.
- In the in-memory backend, `drop(..., true)` erases the store from the backend snapshot.
- In the LMDB backend, `drop(..., true)` forwards directly to the native DB drop call.
- So if room teardown were authoritative, named-store pressure should actually go down instead of only moving around.

Supporting references:
- `src/db/inmemory/InMemoryTxn.cpp:262`
- `src/db/LmdbBackend.cpp:175`

What is the downside of using a larger default `maxStores`?

Relevant references:
- `src/cache/setup/CacheSetup.cpp:94`
- `/usr/include/lmdb.h` (`mdb_env_set_maxdbs` documentation)

Current understanding:
- LMDB's own header says a moderate number of named DB slots is cheap.
- Huge values get expensive because:
  - transaction-local structures scale with the configured slot count
  - each `mdb_dbi_open()` does a linear search across opened slots
- Komai's own code comment already treats values around one million as suspiciously expensive.

Interpretation for Komai:
- Move from `32384` to `131072` by default.
- That should give large-account users substantial headroom and avoid restart pain for room counts around `3000`.
- Still avoid extreme defaults like `1 << 20` without measurement.
- A larger default looks good as a short-term user-facing fix, but not as the only fix.

### 3. Read receipt schema is wrong for current-state semantics

Severity: high

Relevant code:
- `src/db/ReadReceiptIndex.cpp:15`
- `src/cache/lifecycle/CacheLifecycleRooms.cpp:120`
- `src/cache/lifecycle/CacheLifecycleRooms.cpp:150`
- `src/models/ReadReceiptsModel.cpp:20`
- `src/timeline/data/TimelineModelDataRoleHelpers.cpp:189`

What is happening:
- Read receipts are stored as:
  - key: `event_id + room_id`
  - value: `{ user_id: timestamp }`
- `updateReadReceipt(...)` merges new receipts into the event's stored map.
- It never removes the same user from their previous event.

Why this matters:
- Older events can keep stale readers forever.
- The store grows with normal receipt movement.
- Cleanup is awkward because the key shape is event-centric and encoded as JSON.

Probable better design:
- Primary index:
  - `room_id + user_id -> current receipt event/timestamp`
- Secondary index:
  - `room_id + event_id -> users`

That would make:
- advancing a receipt cheap and correct
- room-scoped cleanup much easier
- stale receipt removal explicit

Chosen UI semantics:
- For an older message, "read up to here" is sufficient.
- The UI should be able to answer "who has currently read at least this far?"
- We do not need to store append-only historical per-event residue forever.

### 4. Limited timeline syncs likely leak unreachable event payloads and relation rows

Severity: high

Relevant code:
- `src/cache/timeline/CacheTimelineWrite.cpp:68`
- `src/cache/timeline/CacheTimelineRead.cpp:26`

What is happening:
- On limited timeline sync, Komai drops order/index stores and pending.
- It does not clear `eventsDb` or `relationsDb`.
- Later trimming only walks rooms still present in `db->rooms` and only removes entries reachable through order/index data.

Why this matters:
- Repeated limited syncs can accumulate orphaned event JSON.
- Relation rows can outlive the visible timeline structure.

This may deserve a schema/layout cleanup together with the room-teardown work.

### 5. Relation cleanup is one-sided

Severity: medium

Relevant code:
- `src/db/DupIndex.cpp:43`
- `src/db/timelineindex/TimelineIndexCleanup.cpp:176`
- `src/cache/timeline/CacheTimelineWrite.cpp:31`
- `src/timeline/eventstore/EventStoreRelations.cpp:20`
- `tests/DbBackendTest.cpp:1937`

What is happening:
- Relations are stored as target-event -> related-event IDs.
- Cleanup removes `relationsDb` entries by key for the event being deleted.
- It does not fully clean reverse references from other target buckets.

Why this matters:
- Deleted edits/reactions can leave stale related-event IDs behind.
- Current tests appear to preserve this behavior.

### 6. Space graph indexes are append-biased and may go stale

Severity: medium

Relevant code:
- `src/cache/spaces/CacheSpaces.cpp:40`
- `src/cache/spaces/CacheSpaces.cpp:69`
- `src/cache/spaces/CacheSpaces.cpp:105`

What is happening:
- When a space itself is updated, Komai clears and rebuilds its child edges.
- When a room's parent links change, room-side recomputation only adds current parents.
- It does not clearly remove obsolete old parents for the room.

Why this matters:
- Reparented rooms may remain attached to old spaces.
- Stale spaces can still surface in UI paths.

## Additional Cleanup Scope To Decide

If "remove room" is meant to be truly destructive, the room-local DB drops are not enough by themselves.

There are also room-keyed global stores or room-keyed global values that need an explicit policy:
- `spacesParents` / `spacesChildren`
- `readReceipts`
- `encryptedRooms_`
- `eventExpiryBgJob_`
- `outboundMegolmSessions`

There are also composite-key crypto stores with room IDs embedded in the key:
- `inboundMegolmSessions`
- `megolmSessionsData`

Notes:
- Some of these are clearly room-owned cache state and should probably be removed.
- Some may be intentionally retained for crypto-history reasons.
- This should be decided explicitly as part of the pre-release cache policy rather than left to accidents of current code paths.

Relevant code:
- `src/cache/spaces/CacheSpaces.cpp:44`
- `src/cache/lifecycle/CacheLifecycleRooms.cpp:150`
- `src/cache/crypto/CacheCryptoMegolmMetadata.cpp:23`
- `src/cache/crypto/CacheCryptoMegolm.cpp:132`
- `src/cache/crypto/CacheCryptoMegolm.cpp:23`

Related oddity:
- Any future orphan-store sweep must special-case the synthetic global account-data store created from `getAccountDataDb(txn, "")`.

## Future Work: Cache Stats / Maintenance UI

This is not a launch blocker for the storage rewrite, but it is worth keeping in mind while changing the storage model.

Possible future UI:
- Show cache statistics:
  - LMDB/cache DB size
  - media cache size
  - optional breakdowns if cheap enough to compute
- Offer maintenance actions:
  - clear media cache
  - clear local DB/cache
  - clear everything cache-related
  - compact LMDB/cache DB

Current feasibility notes:
- LMDB compaction already exists today, but only as a startup action via the `--compact` CLI flag.
- Current implementation:
  - opens a temporary DB
  - copies data into it
  - closes both DBs
  - swaps directories
  - reopens the DB
- That is not an in-place live compaction while the app is actively using the DB.

Relevant code:
- `src/app/MainApplication.cpp:153`
- `src/cache/setup/CacheSetup.cpp:139`

Practical implication:
- A future "compact database" UI action may need one of these approaches:
  - restart-required maintenance action
  - shutdown-and-relaunch flow
  - dedicated offline maintenance mode

Design note:
- If the store layout is rewritten now, it is worth keeping future cache-stat collection and cleanup actions in mind so we do not make them harder than necessary.

### 7. Cache version handling is asymmetric

Severity: medium

Relevant code:
- `src/cache/lifecycle/CacheLifecycleMigrations.cpp:69`
- `src/chat/ChatPageBootstrap.cpp:92`

What is happening:
- `formatVersion()` treats only older versions as incompatible.
- Newer-than-current versions are effectively treated as current.

Why this matters:
- Downgrades or mixed builds can try to read a newer cache layout without reset.

Suggested direction:
- Exact-match compatibility only.
- Explicit handling for:
  - older
  - current
  - newer

### 8. Dead schema baggage exists already

Severity: low

Relevant code:
- `src/cache/schema/CacheSchema.h:36`
- `src/cache/setup/CacheSetup.cpp:203`
- `src/db/OrderEntry.cpp:15`

Notes:
- `PendingReceipts` appears unused.
- `OrderEntry` still accepts legacy raw-event-id payloads even though the cache strategy is already "reset incompatible formats".

This is cheap cleanup if a cache-format bump happens anyway.

## Settings Layer

Current conclusion:
- I do not see a must-break-now problem in the YAML settings schema.
- The settings migration path looks basic but coherent.

Relevant code:
- `src/settings/SettingsMigrations.cpp:19`
- `src/settings/SettingsControllerLoad.cpp:52`

Why this looks okay:
- Schema version is tracked per settings scope.
- Future versions are handled safely.
- Migration writeback behavior is explicit.

## Preliminary Recommendations

### Strong candidates before first release

1. Fix room teardown so it drops all room-owned stores.
2. Redesign read receipt storage.
3. Tighten cache format compatibility to exact-match semantics.
4. Remove dead cache schema baggage during the same format bump.

### Likely worth doing if the scope is acceptable

1. Add orphan-store cleanup tooling or startup maintenance.
2. Rework relation cleanup so stale downstream IDs do not survive.
3. Fix stale space-edge handling.

### Bigger structural option

Collapse per-room named DBs into a small number of global stores keyed by room ID plus subkey.

Pros:
- attacks `max_dbs` at the root
- simplifies global cleanup/orphan detection
- reduces sensitivity to room count and churn

Cons:
- bigger change
- touches more persistence code
- should be done with an intentional cache format bump

## Prioritized Recommendation Ladder

### Must fix before first release

1. Make room teardown authoritative.
   - Drop all room-local DBs.
   - Decide explicitly what happens to room-keyed global data.
   - Add tests for joined-room removal, sync-driven left-room cleanup, and the public/UI removal path.
2. Redesign read receipts.
   - The current event-centric schema is not a good representation of current read state.
   - This is still cheap to break now.
3. Make cache compatibility exact-match only.
   - Older and newer cache formats should both be treated explicitly.
4. Do a cache-format bump when the above lands.
   - Remove dead baggage like `PendingReceipts`.
   - Consider dropping legacy order-entry compatibility if no longer needed.

Clarification:
- Because Komai is not released yet, "cache-format bump" should be read as "freely change the on-disk cache layout and reset it if needed".
- This is not a migration burden right now; it is an opportunity to simplify the storage model before compatibility expectations exist.
- The current incompatible-cache reset logic may be sufficient for beta users if the rewrite keeps `sync_state` compatibility useful.
- If not, prefer a cleaner reset approach over adding migration code.

### Should do if scope allows

1. Raise the default `maxStores` immediately as temporary headroom.
   - Set it to `131072`.
   - This reduces user pain while the deeper fixes land.
2. Add an orphan-store sweep during the cache-format bump / startup migration.
   - This would help current users who already accumulated leaked named DBs.
3. Fix relation cleanup so stale downstream relation IDs do not survive.
4. Fix stale space-parent/child edge cleanup.

### Nice to do, but optional before first release

1. Rework the store layout to use a few shared DBs instead of many per-room named DBs.
2. Revisit whether compaction should also act as a garbage-collection pass.

## Minimum Practical Pre-Release Path

If the goal is to reduce risk without taking on the full store-layout rewrite, the smallest path that still seems worthwhile is:

1. Raise default `maxStores` to `131072`.
2. Fix room teardown.
3. Decide and implement cleanup for room-keyed global state.
4. Redesign read receipts.
5. Bump cache format.
6. Add focused tests for teardown, read-receipt advancement, and limited-sync cleanup behavior.

This does not solve the root DBI-per-room design, but it likely removes the biggest correctness and growth risks.

## Next Things To Dig Into

- Verify whether any other room-owned global indexes also need room-scoped cleanup:
  - read receipts
  - notifications
  - encrypted-room metadata
  - event-expiration background job state
  - space edges
- Decide whether room-scoped crypto state should be removed on room teardown or intentionally retained.
- Check whether limited timeline syncs can orphan relation rows in a way that affects UI behavior.
- Verify whether relation cleanup should become bidirectional or reference-counted.
- Validate the `131072` default choice against any practical startup/open-cost concerns if the layout is not changed immediately.
- Check whether an automatic orphan-store sweep can be implemented via `listStoreNames()` safely.
- Add a short prioritized recommendation set once the above is confirmed.

## Open Questions

- Do we want to preserve any left-room timeline history intentionally, or should room removal be strictly destructive?
- If left-room history is worth keeping, should it live in a different schema/layout than active-room timeline state?
- Should compaction also be used as a GC opportunity rather than a pure copy?
- If the cache format is bumped soon, is it worth removing all remaining legacy read/parsing compatibility shims at the same time?
