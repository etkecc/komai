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
// To work around this, the C++ side dispatches `refresh_thread_timeline`
// on each room timeline sync update.  The Refresh handler fetches
// `/relations` from the server directly (bypassing the stale SDK cache)
// and merges the results with the SDK timeline items:
//
//   - Items present in the SDK timeline use the high-quality SDK version
//     (with reactions, delivery state, reply previews, etc.)
//   - Items found in `/relations` but missing from the SDK timeline are
//     converted via a basic converter and added to fill the gap
//   - Items only in the SDK timeline (e.g. local echo) are preserved
//
// This gives the best of both worlds: full SDK features for initial and
// cached events, and live updates for new events delivered by sync.

use super::*;
use super::event_summary::summarize_sync_timeline_event;
use super::timeline_snapshot::{build_room_timeline_snapshot, collect_unavailable_reply_event_ids};

use std::collections::{BTreeMap, HashSet};
use std::time::Duration;

use matrix_sdk::deserialized_responses::TimelineEvent;
use matrix_sdk::ruma::EventId;

/// `/relations` result split into things that become timeline rows
/// (replies, attachments, …) and reaction aggregations to fold onto
/// existing items. Reaction events themselves are filtered out of the
/// items list so they don't render as stray "Reactions updated" rows.
#[derive(Default)]
struct ThreadRelationsData {
    items: Vec<MatrixTimelineItem>,
    /// parent_event_id -> key -> sender_id -> reaction_event_id
    annotations: HashMap<String, HashMap<String, HashMap<String, String>>>,
}

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

// ---------------------------------------------------------------------------
// Background loop
// ---------------------------------------------------------------------------

async fn run_thread_timeline_loop(
    handle_id: u64,
    client: Client,
    room_id: String,
    thread_root_id: String,
    thread_timeline_snapshot: Arc<Mutex<Vec<MatrixTimelineItem>>>,
    room_timeline_media_lookup: Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
    mut commands: mpsc::UnboundedReceiver<ThreadTimelineCommand>,
    stop_requested: Arc<AtomicBool>,
) {
    tracing::info!(handle_id, room_id, thread_root_id, "Running thread timeline loop");

    let parsed_room_id = match RoomId::parse(&room_id) {
        Ok(rid) => rid,
        Err(error) => {
            tracing::warn!(handle_id, room_id, %error, "Invalid room id for thread timeline task");
            return;
        }
    };

    let parsed_thread_root_id = match EventId::parse(&thread_root_id) {
        Ok(eid) => eid,
        Err(error) => {
            tracing::warn!(handle_id, thread_root_id, %error, "Invalid thread root event id");
            return;
        }
    };

    let Some(room) = client.get_room(&parsed_room_id) else {
        tracing::warn!(handle_id, room_id, "Room not known to client for thread timeline");
        return;
    };

    let own_user_id = client.user_id();

    // -----------------------------------------------------------------------
    // Build the SDK TimelineFocus::Thread timeline
    // -----------------------------------------------------------------------
    let focus = TimelineFocus::Thread {
        root_event_id: parsed_thread_root_id.clone(),
    };

    let timeline = match room.timeline_builder().with_focus(focus).build().await {
        Ok(t) => Arc::new(t),
        Err(error) => {
            tracing::warn!(
                handle_id, room_id, thread_root_id, %error,
                "Failed to build thread-focused timeline"
            );
            return;
        }
    };

    let empty_receipts = HashSet::new();
    let (items, stream) = timeline.subscribe().await;
    let mut current_values = items;

    // /relations data for the merge.  Seeded by an initial fetch below so
    // we don't sit on a stale SDK snapshot while waiting for sync to nudge
    // us — `TimelineFocus::Thread` doesn't receive sync events in
    // matrix-sdk 0.16, so without this the view can show only the cached
    // root and miss events posted in another session.
    let mut relations_data = ThreadRelationsData::default();

    // Publish the initial snapshot from the SDK timeline.
    publish_merged_snapshot(
        handle_id, &room_id, &thread_root_id,
        &current_values, own_user_id, &empty_receipts,
        &relations_data,
        &thread_timeline_snapshot,
        &room_timeline_media_lookup,
    );

    // Initial /relations fetch — bypasses the Refresh debounce so the
    // first paint reflects server state, not just whatever the SDK Thread
    // event cache happened to have.
    match fetch_relations_events(&room, &parsed_thread_root_id).await {
        Ok(data) => {
            tracing::info!(
                handle_id, room_id, thread_root_id,
                relations_count = data.items.len(),
                annotation_parents = data.annotations.len(),
                "Initial thread /relations fetch"
            );
            relations_data = data;
            publish_merged_snapshot(
                handle_id, &room_id, &thread_root_id,
                &current_values, own_user_id, &empty_receipts,
                &relations_data,
                &thread_timeline_snapshot,
                &room_timeline_media_lookup,
            );
        }
        Err(error) => {
            tracing::warn!(
                handle_id, room_id, thread_root_id, %error,
                "Initial thread /relations fetch failed"
            );
        }
    }

    // Fetch reply details for events with unavailable reply content.
    let mut reply_detail_fetch_requested: HashSet<OwnedEventId> = HashSet::new();
    request_reply_details(&current_values, &timeline, &mut reply_detail_fetch_requested);

    // Kick off initial backwards pagination to load thread history.
    let initial_paginate_fut = timeline.paginate_backwards(50u16);
    tokio::pin!(initial_paginate_fut);
    let mut initial_paginate_pending = true;

    // Debounce state for Refresh commands.  We wait for a quiet period
    // before actually fetching `/relations` to avoid a flood of rebuilds
    // during rapid room timeline updates (pagination, local echo, etc.).
    let mut refresh_deadline: Option<tokio::time::Instant> = None;
    let far_future = tokio::time::Instant::now() + Duration::from_secs(86400);

    let mut stream = Box::pin(stream);
    while !stop_requested.load(Ordering::Relaxed) {
        tokio::select! {
            result = &mut initial_paginate_fut, if initial_paginate_pending => {
                initial_paginate_pending = false;
                if let Err(error) = result {
                    tracing::warn!(
                        handle_id, room_id, thread_root_id, %error,
                        "Initial thread timeline pagination failed"
                    );
                }
            }

            maybe_diffs = stream.next() => {
                match maybe_diffs {
                    Some(diffs) => {
                        let diffs: Vec<VectorDiff<Arc<TimelineItem>>> = diffs;
                        for diff in diffs.iter().cloned() {
                            diff.apply(&mut current_values);
                        }

                        publish_merged_snapshot(
                            handle_id, &room_id, &thread_root_id,
                            &current_values, own_user_id, &empty_receipts,
                            &relations_data,
                            &thread_timeline_snapshot,
                            &room_timeline_media_lookup,
                        );

                        request_reply_details(&current_values, &timeline, &mut reply_detail_fetch_requested);

                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            diff_count = diffs.len(),
                            "Thread timeline SDK diff"
                        );
                    }
                    None => {
                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            "Thread timeline stream ended"
                        );
                        break;
                    }
                }
            }

            maybe_command = commands.recv() => {
                match maybe_command {
                    Some(ThreadTimelineCommand::PaginateBackwards(page_size)) => {
                        tracing::info!(
                            handle_id, room_id, thread_root_id, page_size,
                            "Paginating thread timeline backwards"
                        );
                        if let Err(error) = timeline.paginate_backwards(page_size).await {
                            tracing::warn!(
                                handle_id, room_id, thread_root_id, page_size, %error,
                                "Failed to paginate thread timeline backwards"
                            );
                            crate::ffi::matrix_notify_thread_timeline_snapshot_updated(
                                handle_id, &room_id, &thread_root_id,
                            );
                        }
                    }
                    Some(ThreadTimelineCommand::Refresh) => {
                        // Debounce: coalesce a burst of Refresh commands
                        // (room-timeline syncs, pagination, …) into a
                        // single /relations fetch. Kept short because
                        // local echoes for thread replies stay stuck
                        // until /relations reconciles them — this is the
                        // upper bound on "React button missing after
                        // sending a thread reply".
                        if refresh_deadline.is_none() {
                            refresh_deadline = Some(
                                tokio::time::Instant::now() + Duration::from_millis(300)
                            );
                        }
                    }
                    None => {
                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            "Thread timeline command channel closed"
                        );
                        break;
                    }
                }
            }

            // Debounced /relations fetch fires when the deadline arrives.
            _ = tokio::time::sleep_until(
                refresh_deadline.unwrap_or(far_future)
            ), if refresh_deadline.is_some() => {
                refresh_deadline = None;

                match fetch_relations_events(
                    &room,
                    &parsed_thread_root_id,
                ).await {
                    Ok(data) => {
                        let prev_count = relations_data.items.len();
                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            relations_count = data.items.len(),
                            annotation_parents = data.annotations.len(),
                            prev_count,
                            "Refreshed thread /relations"
                        );
                        relations_data = data;
                    }
                    Err(error) => {
                        tracing::warn!(
                            handle_id, room_id, thread_root_id, %error,
                            "Failed to fetch thread /relations"
                        );
                    }
                }

                publish_merged_snapshot(
                    handle_id, &room_id, &thread_root_id,
                    &current_values, own_user_id, &empty_receipts,
                    &relations_data,
                    &thread_timeline_snapshot,
                    &room_timeline_media_lookup,
                );
            }
        }
    }

    tracing::info!(handle_id, room_id, thread_root_id, "Thread timeline loop exiting");
}

// ---------------------------------------------------------------------------
// Merged snapshot building
// ---------------------------------------------------------------------------

/// Build a merged snapshot from SDK timeline items and /relations data,
/// then publish it to the shared snapshot and notify C++.
fn publish_merged_snapshot(
    handle_id: u64,
    room_id: &str,
    thread_root_id: &str,
    sdk_values: &Vector<Arc<TimelineItem>>,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
    read_own_event_ids: &HashSet<String>,
    relations_data: &ThreadRelationsData,
    thread_timeline_snapshot: &Arc<Mutex<Vec<MatrixTimelineItem>>>,
    room_timeline_media_lookup: &Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
) {
    let relations_items = &relations_data.items;
    let (mut sdk_items, media_lookup) =
        build_room_timeline_snapshot(sdk_values, own_user_id, read_own_event_ids, None);

    // Fix stale local echoes.  In matrix-sdk 0.16, the Thread-focused
    // timeline never transitions local echoes to remote events (sync
    // events don't flow to the thread event cache).  The SDK does
    // update send_state from NotSentYet → Sent via the send queue, but
    // the item stays as a local echo with a delivery indicator forever.
    //
    // When /relations data is available, replace these stale local
    // echoes with the server-authoritative /relations version (which
    // has no delivery indicator — correct for a delivered remote event).
    if !relations_items.is_empty() {
        for sdk_item in &mut sdk_items {
            if !sdk_item.is_own {
                continue;
            }
            let is_local_echo = matches!(
                sdk_item.delivery_state.as_str(),
                "pending" | "sent"
            );
            if !is_local_echo {
                continue;
            }

            // Match by event_id when it's known (Sent state). Otherwise
            // prefer `unsigned.transaction_id` — the homeserver round-trips
            // the original client-supplied txn_id on events it returns to
            // the sender, so this is a first-party reconciliation key.
            // Fall back to sender + body for servers that strip or omit
            // `unsigned.transaction_id` on /relations responses.
            let matched = if !sdk_item.event_id.is_empty() {
                relations_items.iter().find(|r| r.event_id == sdk_item.event_id)
            } else if !sdk_item.transaction_id.is_empty() {
                relations_items
                    .iter()
                    .find(|r| !r.transaction_id.is_empty()
                        && r.transaction_id == sdk_item.transaction_id)
                    .or_else(|| {
                        relations_items.iter().find(|r| {
                            r.sender_id == sdk_item.sender_id
                                && r.body == sdk_item.body
                                && !r.body.is_empty()
                        })
                    })
            } else {
                relations_items.iter().find(|r| {
                    r.sender_id == sdk_item.sender_id
                        && r.body == sdk_item.body
                        && !r.body.is_empty()
                })
            };

            if let Some(rel_item) = matched {
                *sdk_item = rel_item.clone();
            }
        }
    }

    // sdk_items is reversed (index 0 = newest).  Collect SDK event IDs
    // before merging so we know which /relations items are missing.
    let sdk_event_ids: HashSet<String> = sdk_items
        .iter()
        .filter(|item| !item.event_id.is_empty())
        .map(|item| item.event_id.clone())
        .collect();

    // Also check SDK item_ids (transaction IDs for local echo).
    let sdk_item_ids: HashSet<String> = sdk_items
        .iter()
        .filter(|item| !item.item_id.is_empty())
        .map(|item| item.item_id.clone())
        .collect();

    // Add /relations items that are missing from the SDK timeline.
    let mut added = 0usize;
    for item in relations_items {
        if item.event_id.is_empty() {
            continue;
        }
        if sdk_event_ids.contains(&item.event_id) {
            continue;
        }
        // Skip items whose event_id matches a local echo's item_id
        // (transaction ID) — the SDK version is richer.
        if sdk_item_ids.contains(&item.event_id) {
            continue;
        }
        sdk_items.push(item.clone());
        added += 1;
    }

    if added > 0 {
        // Re-sort: newest first (highest timestamp = index 0).
        sdk_items.sort_by(|a, b| b.timestamp.cmp(&a.timestamp));
    }

    // `transaction_id` on /relations items is populated from the round-tripped
    // `unsigned.transaction_id` purely as a matching key for the local-echo
    // reconciliation above. The QML treats `transactionId.length > 0` as the
    // sole "is local echo" signal (matrix-sdk-ui clears it on remote echo),
    // so leaving it set on a server-confirmed event makes every reply we sent
    // look like a stuck local echo and hides every action that needs an event
    // id (Edit/Reply/React/Forward/...). Clear it for any item that isn't in
    // a transient send state — matching has already happened by this point.
    for item in &mut sdk_items {
        if item.delivery_state.is_empty() && !item.transaction_id.is_empty() {
            item.transaction_id.clear();
        }
    }

    // Fold reaction annotations from /relations onto matching items. The
    // SDK Thread timeline doesn't see reaction sync events in matrix-sdk
    // 0.16 and the raw /relations path doesn't aggregate, so without this
    // a just-sent reaction never shows up as a chip in thread view.
    //
    // This is a *merge*, not an overwrite: /relations is paginated (limit
    // 50 with recurse), so for very long threads it can omit older
    // reactions the SDK already aggregated. Keeping SDK senders preserves
    // those.
    if !relations_data.annotations.is_empty() {
        // Warn when /relations returned a reaction whose parent event isn't
        // in the merged snapshot — that's a silent-failure mode where the
        // chip would never render despite a successful fetch (e.g. a parent
        // that fell outside the 50-event /relations cap, or any future
        // mismatch in how parent_event_id is keyed vs. how we build items).
        for (parent_id, by_key) in &relations_data.annotations {
            if !sdk_items.iter().any(|i| &i.event_id == parent_id) {
                let keys: Vec<&str> = by_key.keys().map(|s| s.as_str()).collect();
                tracing::warn!(
                    parent_event_id = parent_id.as_str(),
                    keys = ?keys,
                    snapshot_size = sdk_items.len(),
                    "Thread reaction bucket has no matching snapshot item"
                );
            }
        }

        for item in &mut sdk_items {
            if item.event_id.is_empty() {
                continue;
            }
            let Some(parent_annotations) = relations_data.annotations.get(&item.event_id) else {
                continue;
            };
            apply_relations_annotations(item, parent_annotations, own_user_id);
        }
    }

    let snapshot_count = sdk_items.len();
    {
        let mut guard = thread_timeline_snapshot
            .lock()
            .expect("poisoned thread timeline snapshot mutex");
        *guard = sdk_items;
    }
    {
        let mut media_guard = room_timeline_media_lookup
            .lock()
            .expect("poisoned room timeline media lookup mutex");
        media_guard.extend(media_lookup);
    }

    if snapshot_count > 0 {
        crate::ffi::matrix_notify_thread_timeline_snapshot_updated(
            handle_id, room_id, thread_root_id,
        );
    }
}

/// Merge `/relations`-derived annotations into an item's `reactions`
/// array. Existing SDK reactions are preserved (their per-sender
/// information may be richer than what we can derive from raw events),
/// and `/relations` adds any senders the SDK hasn't seen — ensuring that
/// a just-sent reaction surfaces as a chip in thread view.
fn apply_relations_annotations(
    item: &mut MatrixTimelineItem,
    annotations: &HashMap<String, HashMap<String, String>>,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
) {
    // Tooltip text stays compact; the full sender list rides on `user_ids`
    // and is consumed by the reaction-details dialog.
    const MAX_TOOLTIP_USERS: usize = 10;

    // Seed per-key sender maps from the existing SDK summary. The
    // SDK-built `users` field is a newline-joined list (truncated with a
    // "… and N more" sentinel beyond MAX_DISPLAYED_USERS) — we recover
    // what we can; the truncated tail is fine to lose since /relations
    // typically supplies the full sender list anyway.
    let mut by_key: BTreeMap<String, BTreeMap<String, String>> = BTreeMap::new();
    let own_str = own_user_id.map(|u| u.as_str().to_owned());

    for reaction in item.reactions.drain(..) {
        let mut senders = BTreeMap::<String, String>::new();
        for line in reaction.users.split('\n') {
            let trimmed = line.trim();
            if trimmed.is_empty() || trimmed.starts_with('…') {
                continue;
            }
            senders.insert(trimmed.to_owned(), String::new());
        }
        // Restore own user's reaction event id so a future redact can
        // target it. `__local__` means it's still a local echo with no
        // server-confirmed event id; treat it as "we reacted, no event id".
        if let Some(own) = own_str.as_deref() {
            if !reaction.self_reacted_event.is_empty() {
                let event_id = if reaction.self_reacted_event == "__local__" {
                    String::new()
                } else {
                    reaction.self_reacted_event.clone()
                };
                senders.insert(own.to_owned(), event_id);
            }
        }
        by_key.entry(reaction.key).or_default().extend(senders);
    }

    for (key, senders) in annotations {
        let entry = by_key.entry(key.clone()).or_default();
        for (sender, reaction_event_id) in senders {
            // Prefer the /relations-supplied reaction event id over an
            // SDK placeholder (empty string) so the redact path has a
            // real target.
            match entry.get(sender) {
                Some(existing) if !existing.is_empty() => continue,
                _ => {
                    entry.insert(sender.clone(), reaction_event_id.clone());
                }
            }
        }
    }

    item.reactions = by_key
        .into_iter()
        .filter(|(_, senders)| !senders.is_empty())
        .map(|(key, senders)| {
            let total = senders.len();
            let user_ids: Vec<String> = senders.keys().cloned().collect();
            let mut users_list: Vec<String> = user_ids
                .iter()
                .take(MAX_TOOLTIP_USERS)
                .cloned()
                .collect();
            if total > MAX_TOOLTIP_USERS {
                users_list.push(format!("… and {} more", total - MAX_TOOLTIP_USERS));
            }
            let users = users_list.join("\n");
            let self_reacted_event = own_str
                .as_deref()
                .and_then(|own| senders.get(own).cloned())
                .unwrap_or_default();
            MatrixReactionSummary {
                key,
                users,
                user_ids,
                self_reacted_event,
                count: total as u64,
            }
        })
        .collect();

    item.reactions_summary = item
        .reactions
        .iter()
        .map(|r| format!("{} {}", r.key, r.count))
        .collect::<Vec<_>>()
        .join("  ");
}

// ---------------------------------------------------------------------------
// /relations fetch + raw event conversion
// ---------------------------------------------------------------------------

/// Fetch thread events from the server via `/relations` and split them
/// into timeline rows + reaction aggregations.  Replies/attachments become
/// basic `MatrixTimelineItem` rows (no SDK-processed reply previews; that's
/// added in `publish_merged_snapshot` if the SDK has the same item).
/// Edits and reactions don't get their own rows — edits fold into the
/// original via `unsigned.relations.replace`, reactions get aggregated
/// onto their parent's `reactions` array during merge.
async fn fetch_relations_events(
    room: &Room,
    thread_root_id: &OwnedEventId,
) -> Result<ThreadRelationsData, String> {
    let opts = matrix_sdk::room::RelationsOptions {
        from: None,
        dir: matrix_sdk::ruma::api::Direction::Backward,
        limit: Some(UInt::from(50u32)),
        include_relations: matrix_sdk::room::IncludeRelations::AllRelations,
        recurse: true,
    };

    let result = room
        .relations(thread_root_id.clone(), opts)
        .await
        .map_err(|e| format!("failed to fetch thread relations: {e}"))?;

    let own_user_id = room.own_user_id().to_owned();

    let mut events = result.chunk;

    // If we got all events (no more pages), also include the thread root
    // event itself — it is not included in /relations results.
    if result.prev_batch_token.is_none() {
        if let Ok(root_event) = room.load_or_fetch_event(thread_root_id, None).await {
            events.push(root_event);
        }
    }

    // Events arrive newest-first from backward pagination.  Reverse to
    // chronological order.
    events.reverse();

    let mut data = ThreadRelationsData::default();
    for event in &events {
        let raw_json = event.raw().json().get();

        // Edits fold into the original via `unsigned.relations.replace`,
        // so an `m.replace` event must not become its own timeline row —
        // it would render as a stray "* …" with the Matrix edit fallback.
        if event_relation_rel_type(raw_json) == Some("m.replace") {
            continue;
        }

        // Reactions become aggregations on the parent's `reactions` array
        // rather than rows of their own. `recurse: true` brings reactions
        // on thread replies into this fetch — the SDK Thread timeline
        // doesn't see them via sync in matrix-sdk 0.16, so this path is
        // how a just-sent reaction surfaces in thread view.
        if let Some(annotation) = parse_annotation_event(raw_json) {
            data.annotations
                .entry(annotation.parent_event_id)
                .or_default()
                .entry(annotation.key)
                .or_default()
                .insert(annotation.sender_id, annotation.reaction_event_id);
            continue;
        }

        if let Some(item) = raw_event_to_timeline_item(event, room, &own_user_id).await {
            data.items.push(item);
        }
    }

    Ok(data)
}

struct ParsedAnnotation {
    parent_event_id: String,
    key: String,
    sender_id: String,
    reaction_event_id: String,
}

/// Parse an `m.reaction` event JSON into its annotation parts. Returns
/// `None` for any event that isn't a usable reaction (wrong type, missing
/// fields, or redacted — redactions strip `content.m.relates_to`).
fn parse_annotation_event(json_str: &str) -> Option<ParsedAnnotation> {
    let parsed: serde_json::Value = serde_json::from_str(json_str).ok()?;

    if parsed.get("type").and_then(|v| v.as_str()) != Some("m.reaction") {
        return None;
    }

    let relates_to = parsed.get("content").and_then(|c| c.get("m.relates_to"))?;
    if relates_to.get("rel_type").and_then(|v| v.as_str()) != Some("m.annotation") {
        return None;
    }

    let parent_event_id = relates_to.get("event_id").and_then(|v| v.as_str())?.to_owned();
    let key = relates_to.get("key").and_then(|v| v.as_str())?.to_owned();
    let sender_id = parsed.get("sender").and_then(|v| v.as_str())?.to_owned();
    let reaction_event_id = parsed
        .get("event_id")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned();

    if parent_event_id.is_empty() || key.is_empty() || sender_id.is_empty() {
        return None;
    }

    Some(ParsedAnnotation {
        parent_event_id,
        key,
        sender_id,
        reaction_event_id,
    })
}

/// Read the `content.m.relates_to.rel_type` of a raw event, if any.
fn event_relation_rel_type(json_str: &str) -> Option<&'static str> {
    // Cheap path: parse just enough to look up the rel_type. We map known
    // values to `'static` strings so callers don't have to deal with
    // owned-vs-borrowed lifetimes.
    let parsed: serde_json::Value = serde_json::from_str(json_str).ok()?;
    let rel_type = parsed
        .get("content")?
        .get("m.relates_to")?
        .get("rel_type")?
        .as_str()?;
    match rel_type {
        "m.replace" => Some("m.replace"),
        "m.annotation" => Some("m.annotation"),
        "m.thread" => Some("m.thread"),
        "m.reference" => Some("m.reference"),
        _ => None,
    }
}

/// Convert a raw `TimelineEvent` into a basic `MatrixTimelineItem`.
/// Produces a functional item with text, sender, media, and timestamps,
/// but without SDK-processed aggregations like reactions or reply previews.
async fn raw_event_to_timeline_item(
    event: &TimelineEvent,
    room: &Room,
    own_user_id: &matrix_sdk::ruma::UserId,
) -> Option<MatrixTimelineItem> {
    let event_id = event.event_id()?.to_string();
    let timestamp = event.timestamp.map(|ts| u64::from(ts.0)).unwrap_or(0);

    let raw = event.raw();
    let deserialized = raw.deserialize().ok()?;

    let sender_id = deserialized.sender().to_string();
    let summary = summarize_sync_timeline_event(&deserialized)?;

    let (sender_display_name, sender_avatar_url) =
        resolve_member_profile(room, deserialized.sender()).await;

    let raw_json = raw.json().get();
    let (thread_root_id, reply_to_event_id) = extract_relations_from_raw(raw_json);

    let is_own = deserialized.sender() == own_user_id;
    // `unsigned.transaction_id` is only populated by the homeserver on
    // events it returns to the original sender, so only read it for our
    // own events — saves a serde parse pass on everyone else's events.
    let transaction_id = if is_own {
        extract_transaction_id_from_raw(raw_json)
    } else {
        String::new()
    };

    // Raw-path items (thread roots loaded via /relations) lack the
    // EventTimelineItem wrapper, so we can only see the wire-level
    // encryption flag, not the shield state. Compute this before the
    // summary fields get moved into the struct literal below.
    let is_encrypted_event =
        event.encryption_info().is_some() || summary.kind == "unable_to_decrypt";

    Some(MatrixTimelineItem {
        item_id: event_id.clone(),
        event_id,
        transaction_id,
        delivery_state: String::new(),
        send_error: String::new(),
        is_recoverable: false,
        thread_id: thread_root_id,
        is_thread_root: summary.is_thread_root,
        thread_reply_count: 0,
        sender_id,
        sender_display_name,
        sender_avatar_url,
        body: summary.body,
        formatted_body: summary.formatted_body,
        reply_event_id: reply_to_event_id,
        reply_sender_id: String::new(),
        reply_sender_display_name: String::new(),
        reply_item_kind: String::new(),
        reply_matrix_event_type: String::new(),
        reply_body: String::new(),
        reply_formatted_body: String::new(),
        reply_media_url: String::new(),
        reply_thumbnail_url: String::new(),
        reply_file_name: String::new(),
        reply_mime_type: String::new(),
        reply_media_width: 0,
        reply_media_height: 0,
        reply_media_duration_ms: 0,
        reply_media_size_bytes: 0,
        reply_blurhash: String::new(),
        reactions: Vec::new(),
        reactions_summary: String::new(),
        special_effect_names: summary.special_effect_names,
        item_kind: summary.kind,
        membership_change_kind: String::new(),
        matrix_event_type: summary.matrix_event_type,
        is_edited: false,
        media_url: summary.media.as_ref().map(|m| m.media_url.clone()).unwrap_or_default(),
        thumbnail_url: summary.media.as_ref().map(|m| m.thumbnail_url.clone()).unwrap_or_default(),
        file_name: summary.media.as_ref().map(|m| m.file_name.clone()).unwrap_or_default(),
        mime_type: summary.media.as_ref().map(|m| m.mime_type.clone()).unwrap_or_default(),
        media_width: summary.media.as_ref().map(|m| m.media_width).unwrap_or(0),
        media_height: summary.media.as_ref().map(|m| m.media_height).unwrap_or(0),
        media_duration_ms: summary.media.as_ref().map(|m| m.media_duration_ms).unwrap_or(0),
        media_size_bytes: summary.media.as_ref().map(|m| m.media_size_bytes).unwrap_or(0),
        blurhash: summary.media.as_ref().map(|m| m.blurhash.clone()).unwrap_or_default(),
        media_is_encrypted: summary.media.as_ref().map(|m| m.media_is_encrypted).unwrap_or(false),
        thumbnail_is_encrypted: summary.media.as_ref().map(|m| m.thumbnail_is_encrypted).unwrap_or(false),
        is_voice_message: summary.is_voice_message,
        waveform: summary.waveform,
        timestamp,
        is_own,
        state_event_target_user: String::new(),
        state_event_target_user_id: String::new(),
        state_event_detail: String::new(),
        state_event_reason: String::new(),
        state_event_has_sender: false,
        utd_cause: summary.utd_cause,
        is_encrypted_event,
        // Shield tags left empty on the raw path; the UI treats that as
        // "no shield" (verified/clean), which is a safe default for a
        // thread-root preview fetched out-of-band.
        shield_color: String::new(),
        shield_code: String::new(),
        power_level_changes: Vec::new(),
        server_acl_changes: None,
    })
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Resolve a user's display name and avatar URL from room membership.
async fn resolve_member_profile(
    room: &Room,
    user_id: &matrix_sdk::ruma::UserId,
) -> (String, String) {
    match room.get_member_no_sync(user_id).await {
        Ok(Some(member)) => (
            member.display_name().unwrap_or_default().to_owned(),
            member
                .avatar_url()
                .map(|u| normalize_mxc_uri(u.to_string()))
                .unwrap_or_default(),
        ),
        _ => (user_id.to_string(), String::new()),
    }
}

/// Extract thread root ID and reply-to event ID from raw event JSON.
fn extract_relations_from_raw(json_str: &str) -> (String, String) {
    let parsed: serde_json::Value = match serde_json::from_str(json_str) {
        Ok(v) => v,
        Err(_) => return (String::new(), String::new()),
    };

    let relates_to = match parsed.get("content").and_then(|c| c.get("m.relates_to")) {
        Some(r) => r,
        None => return (String::new(), String::new()),
    };

    let thread_root_id = if relates_to.get("rel_type").and_then(|v| v.as_str()) == Some("m.thread")
    {
        relates_to
            .get("event_id")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_owned()
    } else {
        String::new()
    };

    let reply_to_event_id = relates_to
        .get("m.in_reply_to")
        .and_then(|r| r.get("event_id"))
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned();

    (thread_root_id, reply_to_event_id)
}

/// Extract the original client-supplied transaction id from a raw event's
/// `unsigned.transaction_id` field. Per the Matrix spec, homeservers populate
/// this on events they return to the original sender after a successful PUT,
/// which lets us reconcile a just-sent local echo with its server-confirmed
/// /relations twin without guessing by sender + body.
fn extract_transaction_id_from_raw(json_str: &str) -> String {
    let parsed: serde_json::Value = match serde_json::from_str(json_str) {
        Ok(v) => v,
        Err(_) => return String::new(),
    };

    parsed
        .get("unsigned")
        .and_then(|u| u.get("transaction_id"))
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned()
}

/// Fire-and-forget tasks to fetch reply details for events with unavailable
/// reply content.
fn request_reply_details(
    values: &Vector<Arc<TimelineItem>>,
    timeline: &Arc<Timeline>,
    requested: &mut HashSet<OwnedEventId>,
) {
    for event_id in collect_unavailable_reply_event_ids(values) {
        if requested.insert(event_id.clone()) {
            let timeline_clone = timeline.clone();
            tokio::spawn(async move {
                if let Err(e) = timeline_clone.fetch_details_for_event(&event_id).await {
                    tracing::info!("Failed to fetch thread reply details for {}: {e}", event_id);
                }
            });
        }
    }
}
