# Thread Reply Counts

Thread root messages in the main timeline display a reply count badge showing
how many messages the thread contains.  Getting accurate counts is non-trivial
because the SDK's built-in mechanism (`ThreadSummary.num_replies`) requires
enabling `ThreadingSupport`, which has unacceptable side effects.

## Why not `ThreadingSupport::Enabled`?

The matrix-sdk ties two behaviors to a single `ThreadingSupport` flag:

1. **Event cache thread summary computation** -- recomputes reply counts when
   new thread replies arrive via sync.
2. **Read receipt exclusion** -- thread replies are excluded from room-level
   `num_unread`, `num_notifications`, and `num_mentions` counts.

Behavior (2) breaks room unread badges for thread replies.  In rooms where bots
reply in threads, or where @-mentions happen inside threads, the room would
never show as unread.  There is no per-thread unread mechanism available in
sliding sync to compensate (`unread_thread_notifications` exists only in sync
v3 responses).

## Solution: `Room::list_threads()` cache

Instead of enabling `ThreadingSupport`, we fetch accurate reply counts from the
server via `Room::list_threads()` (which calls `GET /rooms/{roomId}/threads`)
and cache them in memory.

### Cache structure

```
MatrixBackendHandle
  thread_reply_counts: Arc<Mutex<HashMap<RoomId, HashMap<EventId, u32>>>>
```

Flat in-memory HashMap, not persisted.  Rebuilt from the server on each session.

### Population

When a room's timeline loop starts (`run_room_timeline_loop` in
`runtime_timeline.rs`), a background task is spawned after the initial snapshot:

1. Call `room.list_threads()` (first page, server default limit).
2. For each returned event, extract `event_id` and
   `thread_summary.num_replies`.
3. Insert into the shared cache.
4. Rebuild the timeline snapshot with the new counts and notify the C++ side.

This does not block the initial timeline paint -- the timeline appears
immediately with 0 counts, then updates within ~200ms when the cache is ready.

### Incremental updates

When timeline diffs arrive via sync, the loop scans for new thread reply events
(items with a non-empty `thread_root`).  Each new reply increments the cached
count for its thread root.  A `HashSet<String>` of seen reply event IDs prevents
double-counting across diff batches.

### Consumption

`build_room_timeline_snapshot()` accepts an optional `&HashMap<String, u32>`
parameter.  For each timeline item whose `event_id` appears in the cache:

- `is_thread_root` is set to `true`
- `thread_reply_count` is overridden with the cached value (if the SDK's own
  count is 0, which is the common case)

### C++ `isThreadRoot` derivation

The C++ model (`MatrixTimelineModel.cpp`) independently derives `isThreadRoot`
by scanning all items and checking which `eventId` values appear as another
item's `threadId`.  This catches thread roots not yet in the `list_threads()`
cache (e.g., very new threads).  The Rust-side cache augments this with accurate
server-side counts.

## Data flow

```
  Room::list_threads()          sync diffs (new thread replies)
         |                              |
         v                              v
  thread_reply_counts cache    increment cached counts
         |                              |
         +--------- merge ------------>-+
                       |
                       v
         build_room_timeline_snapshot()
           (applies cached counts to items)
                       |
                       v
              MatrixTimelineItem
           { is_thread_root, thread_reply_count }
                       |
                       v
              C++ MatrixTimelineModel
           (also derives isThreadRoot locally)
                       |
                       v
              QML thread reply badge
```

## Key files

| File | Role |
|---|---|
| `src/rust/src/matrix_backend/runtime.rs` | `thread_reply_counts` field on `MatrixBackendHandle` |
| `src/rust/src/matrix_backend/runtime_timeline.rs` | Cache population (`list_threads()` spawn), incremental updates, passes cache to snapshot builder |
| `src/rust/src/matrix_backend/runtime_timeline_snapshot.rs` | Applies cached counts in `build_room_timeline_snapshot()` |
| `src/timeline/rust/MatrixTimelineModel.cpp` | C++ `isThreadRoot` derivation (independent, complementary) |

## Limitations

- **First page only**: Only the most recent threads (server default limit,
  typically ~20) are fetched.  Very old threads in long-scrollback rooms may
  show 0 until the Threads dialog is opened.
- **Count accuracy**: Incremental counts may drift slightly from the server's
  true count if events are redacted or if the initial `list_threads()` response
  was stale.  Re-entering the room re-fetches from the server.
- **No per-thread unread tracking**: The cache tracks total reply counts, not
  "unread" counts per thread.  Per-thread unread indicators would require
  either sync v3 `unread_thread_notifications` or custom per-thread read
  receipt tracking.
