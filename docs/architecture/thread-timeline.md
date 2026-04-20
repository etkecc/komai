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

On each room timeline sync update, the C++ side dispatches a `Refresh` command
to the thread timeline loop.  After a 1.5-second debounce (to coalesce rapid
updates from pagination and local echo), the loop fetches events from the server
via the `/relations` endpoint, bypassing the SDK's stale thread event cache.

### Merged snapshot

Each snapshot published to the C++ model merges both sources:

1. SDK timeline items are built via `build_room_timeline_snapshot` (same pipeline
   as the room timeline)
2. /relations items not present in the SDK timeline are converted via a basic
   converter and added to fill gaps
3. SDK items with stale delivery state (pending/sent) are replaced by the
   /relations version when the server confirms delivery

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
  │    - command channel            │ ◄── PaginateBackwards / Refresh
  │    - debounced /relations timer │
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
for the active room while in thread mode.

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

- **Reactions on new events**: Events that arrive only via /relations (not in
  the SDK timeline's initial load) lack reaction aggregations.  Reactions added
  after the initial load are not visible until the thread is re-entered.

- **Reply previews on new events**: Events from /relations have reply-to event
  IDs but sender/body are not resolved (the SDK's `fetch_details_for_event`
  only works on SDK timeline items).

- **~1.5s live update delay**: The debounce window means sync-delivered events
  appear ~1.5 seconds after arrival, not instantly.

- **50-event /relations limit**: The refresh fetch uses `limit: 50`.  Threads
  longer than 50 messages may not show all recent events via /relations.  The
  SDK pagination command is available for loading older events.

- **Local echo delivery indicator timing**: Own messages show a delivery
  indicator (pending -> sent) until the next /relations refresh confirms
  delivery (~1.5s after sync).

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
