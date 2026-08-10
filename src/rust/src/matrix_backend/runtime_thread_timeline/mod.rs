// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Thread timeline view — displays a single thread's messages in the
// timeline area instead of the full room timeline.
//
// # Architecture: SDK timeline + /relations hybrid
//
// Uses `TimelineFocus::Thread` to build a dedicated SDK Timeline that
// processes events through the full timeline processing pipeline.  This
// gives us reactions, local echo, delivery state, reply previews, and
// edit support on the initial thread events.
//
// However, in matrix-sdk 0.16 the `TimelineFocus::Thread` timeline does
// NOT reliably receive events from sync — `room_event_cache_updates_task`
// only calls `handle_remote_aggregations()` for non-Live focuses, and
// the thread event cache's pagination state persists across Timeline
// instances (so rebuilding doesn't help either).
//
// To work around this, two paths feed `refresh_thread_timeline`:
//
//   1. The C++ side dispatches `refresh_thread_timeline` on each room
//      timeline sync update — covers events the SDK Live timeline
//      surfaces (e.g. new top-level messages, latest_thread_summary
//      bumps from new thread replies).
//   2. The thread loop subscribes directly to `RoomEventCache` updates
//      from the same room — covers events that flow into the room cache
//      but never reach the Live timeline (most importantly, reactions on
//      thread messages: their parent is in-thread, so the Live timeline
//      emits no diff and path 1 never fires).
//
// Both paths feed the same debounced refresh, which fetches `/relations`
// from the server directly (bypassing the stale SDK cache) and merges the
// results with the SDK timeline items:
//
//   - Items present in the SDK timeline use the high-quality SDK version
//     (with reactions, delivery state, reply previews, etc.)
//   - Items found in `/relations` but missing from the SDK timeline are
//     converted via a basic converter and added to fill the gap
//   - Items only in the SDK timeline (e.g. local echo) are preserved
//
// After every `/relations` fetch, `augment_annotations_from_room_cache`
// mines the persisted room event cache for any reaction events Synapse
// dropped from `/relations` (observed: Synapse's recurse=true silently
// omits some reactions on thread reply messages even though the events
// exist on the server). The cache reactions deduplicate against
// `/relations` on `(parent, key, sender)`, so this is purely additive.
//
// This gives the best of both worlds: full SDK features for initial and

mod items;
mod read;
mod relations;
mod snapshot;
mod task_loop;

pub(super) use items::extract_relations_from_raw;
pub(in crate::matrix_backend) use items::raw_event_to_timeline_item;

use std::sync::atomic::Ordering;

use matrix_sdk::ruma::EventId;

use super::*;
use task_loop::run_thread_timeline_loop;

/// Per-handle state for an active thread view.
pub struct ThreadTimelineState {
    pub room_id: String,
    pub thread_root_id: String,
    pub snapshot: Arc<Mutex<Vec<MatrixTimelineItem>>>,
    task: Option<ThreadTimelineTask>,
}

struct ThreadTimelineTask {
    commands: mpsc::UnboundedSender<ThreadTimelineCommand>,
    stop_requested: Arc<AtomicBool>,
    #[allow(dead_code)]
    thread: std::thread::JoinHandle<()>,
}

enum ThreadTimelineCommand {
    PaginateBackwards(u16),
    Refresh,
}

// ---------------------------------------------------------------------------
// Public API called from FFI wrappers
// ---------------------------------------------------------------------------

/// Set up a `TimelineFocus::Thread` timeline for the given thread and
/// spawn a background loop that processes VectorDiff updates.
///
/// On a warm cache hit (the same `(room_id, thread_root_id)` was visited
/// recently and not evicted), reuses the existing background task and its
/// merged snapshot — no SDK rebuild, no fresh `/relations` round-trip on
/// the critical path. The cached snapshot is delivered to C++ via an
/// immediate notify, and a `Refresh` is queued so the next `/relations`
/// data lands shortly without blocking the re-entry.
pub fn subscribe_to_thread_timeline(
    handle_id: u64,
    room_id: &str,
    thread_root_id: &str,
) -> Result<(), String> {
    let room_id = room_id.trim();
    let thread_root_id = thread_root_id.trim();
    if room_id.is_empty() || thread_root_id.is_empty() {
        return Err("room_id and thread_root_id must not be empty".to_owned());
    }

    let _ = EventId::parse(thread_root_id)
        .map_err(|e| format!("invalid thread root event id '{thread_root_id}': {e}"))?;

    let key = (room_id.to_owned(), thread_root_id.to_owned());

    // Fast path: the subscription is already cached.  Mark it active, bump
    // it in the LRU, send a Refresh, and notify C++ so it pulls the cached
    // snapshot right away.
    let cache_hit = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles.get_mut(&handle_id).ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} is not active")
        })?;

        if handle.thread_subscriptions.contains_key(&key) {
            handle.active_thread_key = Some(key.clone());
            touch_lru(&mut handle.thread_subscription_lru, &key);
            // Snag the command sender for the warm-path Refresh.  Send while
            // holding the registry lock would deadlock the bg loop if it
            // tries to take the lock — but the channel is unbounded and the
            // send returns synchronously without entering the loop.
            handle
                .thread_subscriptions
                .get(&key)
                .and_then(|sub| sub.task.as_ref().map(|t| t.commands.clone()))
        } else {
            None
        }
    };

    if let Some(commands) = cache_hit {
        let _ = commands.send(ThreadTimelineCommand::Refresh);
        crate::ffi::matrix_notify_thread_timeline_snapshot_updated(
            handle_id,
            room_id,
            thread_root_id,
        );
        tracing::info!(
            handle_id,
            room_id,
            thread_root_id,
            "Reactivated cached thread timeline subscription"
        );
        return Ok(());
    }

    // Cold path: build a fresh subscription, evict the LRU victim if
    // capacity is exceeded.
    let (client, snapshot, room_timeline_media_lookup, evicted) = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles.get_mut(&handle_id).ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} is not active")
        })?;

        let snapshot = Arc::new(Mutex::new(Vec::new()));

        handle.thread_subscriptions.insert(
            key.clone(),
            ThreadTimelineState {
                room_id: room_id.to_owned(),
                thread_root_id: thread_root_id.to_owned(),
                snapshot: Arc::clone(&snapshot),
                task: None,
            },
        );
        handle.active_thread_key = Some(key.clone());
        touch_lru(&mut handle.thread_subscription_lru, &key);

        let evicted = drain_lru_overflow(handle);

        (
            handle.client.clone(),
            snapshot,
            Arc::clone(&handle.room_timeline_media_lookup),
            evicted,
        )
    };

    // Stop evicted tasks outside the registry lock — `stop_thread_task`
    // joins the bg thread, which itself acquires the registry lock when
    // publishing snapshots; holding the lock here would deadlock.
    for state in evicted {
        stop_thread_task(handle_id, state);
    }

    let stop_requested = Arc::new(AtomicBool::new(false));
    let stop_requested_for_thread = Arc::clone(&stop_requested);
    let (command_sender, command_receiver) = mpsc::unbounded_channel();
    let room_id_owned = room_id.to_owned();
    let thread_root_id_owned = thread_root_id.to_owned();

    let task_thread = std::thread::spawn(move || {
        crate::matrix_backend::ffi::runtime().block_on(run_thread_timeline_loop(
            handle_id,
            client,
            room_id_owned,
            thread_root_id_owned,
            snapshot,
            room_timeline_media_lookup,
            command_receiver,
            stop_requested_for_thread,
        ));
    });

    {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        if let Some(handle) = handles.get_mut(&handle_id) {
            if let Some(sub) = handle.thread_subscriptions.get_mut(&key) {
                sub.task = Some(ThreadTimelineTask {
                    commands: command_sender,
                    stop_requested,
                    thread: task_thread,
                });
            }
        }
    }

    tracing::info!(
        handle_id,
        room_id,
        thread_root_id,
        "Initialized thread timeline with TimelineFocus::Thread"
    );

    Ok(())
}

/// Move `key` to the most-recently-used position in the LRU list.
fn touch_lru(lru: &mut Vec<(String, String)>, key: &(String, String)) {
    if let Some(pos) = lru.iter().position(|k| k == key) {
        lru.remove(pos);
    }
    lru.push(key.clone());
}

/// Pop and return cache entries that fall outside the capacity budget,
/// preserving the active key (so the user's current view is never the
/// eviction victim, even if it's somehow at the LRU front — defensive).
fn drain_lru_overflow(
    handle: &mut MatrixBackendHandle,
) -> Vec<ThreadTimelineState> {
    let mut evicted = Vec::new();
    while handle.thread_subscriptions.len() > THREAD_SUBSCRIPTION_CACHE_CAP {
        // Pick the oldest entry that isn't the active one.
        let victim_idx = handle.thread_subscription_lru.iter().position(|k| {
            handle.active_thread_key.as_ref().map_or(true, |a| a != k)
        });
        let Some(idx) = victim_idx else { break };
        let victim_key = handle.thread_subscription_lru.remove(idx);
        if let Some(state) = handle.thread_subscriptions.remove(&victim_key) {
            tracing::info!(
                room_id = %state.room_id,
                thread_root_id = %state.thread_root_id,
                "Evicting thread timeline subscription (LRU)"
            );
            evicted.push(state);
        }
    }
    evicted
}

/// Tear down a single cached `ThreadTimelineState` without touching the
/// registry: signal stop and drop the command channel so the bg loop
/// observes the shutdown.
fn stop_thread_task(handle_id: u64, state: ThreadTimelineState) {
    if let Some(task) = state.task {
        task.stop_requested.store(true, Ordering::Relaxed);
        drop(task.commands);
        // Don't join — the loop may still be flushing a final publish; we
        // already dropped our reference to the snapshot, so it'll clean up
        // on its own.  This matches the room-timeline shutdown pattern.
        let _ = task.thread;
    }
    tracing::info!(
        handle_id,
        room_id = %state.room_id,
        thread_root_id = %state.thread_root_id,
        "Stopped thread timeline subscription"
    );
}

/// Drop every cached thread subscription whose room matches `room_id`.
/// Called from `stop_room_timeline` so a room leaving the cache also
/// purges its threads.
pub fn evict_thread_subscriptions_for_room(handle_id: u64, room_id: &str) {
    let evicted = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get_mut(&handle_id) else {
            return;
        };
        let keys: Vec<(String, String)> = handle
            .thread_subscriptions
            .keys()
            .filter(|(r, _)| r == room_id)
            .cloned()
            .collect();
        let mut evicted = Vec::with_capacity(keys.len());
        for key in keys {
            if let Some(state) = handle.thread_subscriptions.remove(&key) {
                evicted.push(state);
            }
            handle.thread_subscription_lru.retain(|k| k != &key);
            if handle.active_thread_key.as_ref() == Some(&key) {
                handle.active_thread_key = None;
            }
        }
        evicted
    };

    for state in evicted {
        stop_thread_task(handle_id, state);
    }
}

/// Fetch the current in-memory thread timeline snapshot for the active
/// thread (the one most recently passed to `subscribe_to_thread_timeline`).
pub async fn fetch_thread_timeline_snapshot(
    handle_id: u64,
) -> Result<Vec<MatrixTimelineItem>, String> {
    let snapshot = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .and_then(|handle| {
            handle
                .active_thread_key
                .as_ref()
                .and_then(|key| handle.thread_subscriptions.get(key))
                .map(|sub| {
                    sub.snapshot
                        .lock()
                        .expect("poisoned thread timeline snapshot mutex")
                        .clone()
                })
        })
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no active thread timeline"
            )
        })?;

    Ok(snapshot)
}

/// Request backwards pagination by sending a command to the background loop.
pub async fn paginate_thread_timeline_backwards(
    handle_id: u64,
    num_events: u16,
) -> Result<bool, String> {
    send_thread_command(handle_id, ThreadTimelineCommand::PaginateBackwards(num_events))
}

/// Signal the thread timeline loop to refresh by fetching `/relations`.
pub fn refresh_thread_timeline(handle_id: u64) -> Result<(), String> {
    send_thread_command(handle_id, ThreadTimelineCommand::Refresh).map(|_| ())
}

/// Mark the active thread as no longer in view.  The cached subscription
/// is *kept* so a subsequent re-entry skips the SDK rebuild and
/// `/relations` round-trip; eviction happens via LRU pressure on a new
/// subscribe, or when the room timeline itself is stopped (see
/// `evict_thread_subscriptions_for_room`).
pub fn unsubscribe_from_thread_timeline(handle_id: u64) -> Result<(), String> {
    let mut handles = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex");
    if let Some(handle) = handles.get_mut(&handle_id) {
        if let Some(key) = handle.active_thread_key.take() {
            tracing::info!(
                handle_id,
                room_id = %key.0,
                thread_root_id = %key.1,
                "Deactivated thread timeline (cached for reuse)"
            );
        }
    }
    Ok(())
}

fn send_thread_command(handle_id: u64, command: ThreadTimelineCommand) -> Result<bool, String> {
    let command_sender = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles.get(&handle_id).ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} is not active")
        })?;
        let key = handle.active_thread_key.as_ref().ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} has no active thread timeline")
        })?;
        let sub = handle.thread_subscriptions.get(key).ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} active thread timeline missing from cache")
        })?;
        let task = sub.task.as_ref().ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} thread timeline task not running")
        })?;
        task.commands.clone()
    };

    command_sender
        .send(command)
        .map_err(|_| "failed to send command to thread timeline".to_owned())?;

    Ok(true)
}
