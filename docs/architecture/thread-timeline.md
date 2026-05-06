# Thread Timeline

The thread timeline view replaces the room timeline with a single thread's messages,
allowing the user to read and reply within the thread without leaving the room.

## Architecture: SDK timeline + /relations hybrid

The implementation combines two data sources to work around a limitation in
matrix-sdk 0.16 where `TimelineFocus::Thread` timelines do not reliably receive
events delivered by sync.

### SDK timeline (TimelineFocus::Thread)

A dedicated SDK Timeline built with `TimelineFocus::Thread` stays alive for the
entire thread view session.  Events flow through the SDK's full timeline processing
pipeline, providing:

- Reactions and reaction aggregation on initial/cached events
- Local echo via send queue subscription (messages appear instantly)
- Delivery state transitions (pending -> sent) via send queue
- Reply preview resolution via `fetch_details_for_event`
- Edit support on cached events
- Proper sender profile resolution (display name, avatar)

The SDK timeline is never rebuilt during the session.

### /relations refresh (live updates)

Two paths feed the same debounced `/relations` refresh:

1. **C++ → Refresh command.** On each room timeline sync update, the C++ side
   dispatches a `Refresh` command to the thread timeline loop. This catches
   anything that surfaces as a Live timeline diff (new top-level messages,
   `latest_thread_summary` bumps from new thread replies).
2. **Room event cache subscription.** The thread loop subscribes directly to
   `RoomEventCache::subscribe()` for the same room and schedules a refresh
   on every `UpdateTimelineEvents` from `EventsOrigin::Sync`. This catches
   events that flow into the room cache but never reach the Live timeline —
   most importantly **reactions on thread reply messages**, whose parent is
   in-thread, so the Live timeline emits no diff and path 1 never fires.

Both paths converge on a single 300 ms debounce, so coincident triggers
collapse into one `/relations` fetch.

The debounce is short on purpose: thread reply local echoes never transition
to remote echoes in matrix-sdk 0.16 (sync events don't reach
`TimelineFocus::Thread`), so the QML treats them as local echoes — with no
React/Edit/Reply buttons — until the `/relations` merge replaces them with
their server-confirmed twin. Every additional millisecond of debounce is a
millisecond the user can't react to a just-sent thread reply.

### Cache backfill for missing reactions

Synapse's `GET /_matrix/client/v1/rooms/{roomId}/relations/{eventId}` with
`recurse=true` is observed to silently drop some reactions on thread reply
events — same homeserver, same user, no redaction. The reaction event is
still present in the persisted room event cache (and visible in the main
timeline, which aggregates from there).

After every `/relations` fetch, the loop calls
`RoomEventCache::find_event_relations(item, RelationType::Annotation)` for
every item in the thread snapshot (and the root) and folds any reactions
Synapse omitted into the annotation map. The map is keyed by
`(parent_event_id, key, sender)` so reactions present in both `/relations`
and the cache deduplicate naturally.

This is purely a workaround for a server-side shortfall; if Synapse's
`/relations` becomes complete, the backfill becomes a no-op (silent in
logs unless it actually adds something).

### Merged snapshot

Each snapshot published to the C++ model merges both sources:

1. SDK timeline items are built via `build_room_timeline_snapshot` (same pipeline
   as the room timeline)
2. /relations items not present in the SDK timeline are converted via a basic
   converter and added to fill gaps
3. SDK items with stale delivery state (pending/sent) are replaced by the
   /relations version when the server confirms delivery
4. Reaction annotations from /relations (`m.reaction` events with
   `m.annotation` rel_type) are folded onto matching parents'
   `reactions` arrays — they don't become standalone rows. The merge
   takes the union of SDK senders and /relations senders, so /relations'
   pagination limit (50 with recurse) doesn't drop reactions that the
   SDK already aggregated. Edits (`m.replace`) are dropped from the
   /relations row stream too — they fold into the original via
   `unsigned.relations.replace`.

SDK items take priority when both sources have the same event, preserving the
richer data (reactions, reply previews, delivery state).

## Data flow

```
  ┌─────────────────────────────────┐
  │ subscribe_to_thread_timeline()  │
  │  • build TimelineFocus::Thread  │
  │  • subscribe to diff stream     │
  │  • spawn background loop        │
  └──────────┬──────────────────────┘
             │
  ┌──────────▼──────────────────────┐
  │ Thread timeline loop (Rust)     │
  │  • tokio::select! on:           │
  │    - SDK diff stream            │
  │    - room event cache subscriber│ ◄── reactions on thread messages
  │    - command channel            │ ◄── PaginateBackwards / Refresh
  │    - debounced /relations timer │
  │  • /relations + cache backfill  │
  │  • merge SDK + /relations items │
  │  • publish snapshot, notify C++ │
  └──────────┬──────────────────────┘
             │ matrix_notify_thread_timeline_snapshot_updated
  ┌──────────▼──────────────────────┐
  │ C++ handler                     │
  │  • fetch snapshot on worker     │
  │  • replaceItems() on model      │
  └──────────┬──────────────────────┘
             │
  ┌──────────▼──────────────────────┐
  │ QML ListView                    │
  │  model: threadTimelineModel     │
  └─────────────────────────────────┘
```

The C++ side triggers `Refresh` on each `handleMatrixBackendRoomTimelineSnapshotUpdated`
for the active room while in thread mode. The Rust loop also independently
schedules a refresh whenever the room event cache emits a sync update — the
two converge on the same debounce.

## Thread entry/exit and delegate recycling

The QML delegate binds its `EventDelegateChooser.room` to whichever model the
ListView is showing — `threadTimelineModel` in thread view, `perRoomModel`
otherwise. Because `EventDelegateChooser::setRoom` re-incubates the inner
child, the transition itself flushes any stale content in recycled delegates
without a full model reset. This also avoids the local-echo race where the
per-room snapshot lags the thread diff by ~100 ms and the chooser would
otherwise resolve the echo's id against a model that didn't have it yet.

## SDK limitation: sync events and thread timelines

In matrix-sdk 0.16, `TimelineFocus::Thread` timelines have two limitations:

1. **No sync event routing**: `room_event_cache_updates_task` calls
   `handle_remote_aggregations()` (not `handle_remote_events_with_diffs()`)
   for non-Live focuses.  New messages from sync are not added to the thread
   timeline.

2. **Persistent thread cache state**: The thread event cache maintains pagination
   state across Timeline instances.  Rebuilding a Timeline reuses the same stale
   cache, so new events are not discovered via pagination either.

The /relations hybrid works around both limitations by fetching directly from the
server.

## Remaining limitations

- **Reply previews on new events**: Events from /relations have reply-to event
  IDs but sender/body are not resolved (the SDK's `fetch_details_for_event`
  only works on SDK timeline items).

- **~300 ms live update delay**: The debounce window means sync-delivered
  events (and reactions on them) appear ~300 ms after arrival, not instantly.

- **50-event /relations limit**: The refresh fetch uses `limit: 50` with
  recursion, covering replies, edits, and reactions in the same response.
  In threads where this cap is hit, older reactions still surface from
  whatever the SDK had — the merge takes the union of senders, not just
  the /relations slice. The cache backfill (above) also covers reactions
  Synapse drops outright, regardless of pagination.

- **Local echo delivery indicator timing**: Own messages show a delivery
  indicator (pending -> sent) until the next /relations refresh confirms
  delivery (~300 ms after sync).

## Key files

| Layer | File | Purpose |
|---|---|---|
| Rust | `src/rust/src/matrix_backend/runtime_thread_timeline.rs` | Thread timeline loop, /relations fetch, merged snapshot building |
| Rust | `src/rust/src/matrix_backend/runtime_timeline_snapshot.rs` | `build_room_timeline_snapshot` (shared with room timeline) |
| Rust | `src/rust/src/matrix_backend/runtime.rs` | Module registration, `ThreadTimelineState` in handle, `TimelineFocus` import |
| FFI | `src/rust/src/ffi.rs` | CXX bridge declarations |
| FFI | `src/rust/src/matrix_backend/ffi/active_timeline.rs` | FFI wrappers |
| C++ | `src/timeline/view/TimelineViewManagerMatrixTimeline.cpp` | Subscribe/refresh/clear, notification handler, pagination, thread exit |
| QML | `resources/qml/timeline/components/MatrixRoomView.qml` | Model swap (`threadViewActive`), chooser `room` binding, thread pagination trigger |
