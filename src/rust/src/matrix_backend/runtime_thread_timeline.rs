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

use std::collections::HashSet;
use std::time::Duration;

use matrix_sdk::deserialized_responses::TimelineEvent;
use matrix_sdk::ruma::EventId;

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

    let (client, snapshot, room_timeline_media_lookup) = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles.get_mut(&handle_id).ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} is not active")
        })?;

        let snapshot = Arc::new(Mutex::new(Vec::new()));

        handle.active_thread_subscription = Some(ThreadTimelineState {
            room_id: room_id.to_owned(),
            thread_root_id: thread_root_id.to_owned(),
            snapshot: Arc::clone(&snapshot),
            task: None,
        });

        (
            handle.client.clone(),
            snapshot,
            Arc::clone(&handle.room_timeline_media_lookup),
        )
    };

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
            if let Some(sub) = handle.active_thread_subscription.as_mut() {
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

/// Fetch the current in-memory thread timeline snapshot.
pub async fn fetch_thread_timeline_snapshot(
    handle_id: u64,
) -> Result<Vec<MatrixTimelineItem>, String> {
    let snapshot = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .and_then(|handle| {
            handle.active_thread_subscription.as_ref().map(|sub| {
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

/// Clear thread timeline state and stop the background loop.
pub fn unsubscribe_from_thread_timeline(handle_id: u64) -> Result<(), String> {
    let mut handles = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex");
    if let Some(handle) = handles.get_mut(&handle_id) {
        if let Some(sub) = handle.active_thread_subscription.take() {
            if let Some(task) = sub.task {
                task.stop_requested.store(true, Ordering::Relaxed);
                drop(task.commands);
            }
            tracing::info!(
                handle_id,
                room_id = %sub.room_id,
                thread_root_id = %sub.thread_root_id,
                "Cleared thread timeline state"
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
        let sub = handle.active_thread_subscription.as_ref().ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} has no active thread timeline")
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

    // /relations items for the merge.  Starts empty — the initial data comes
    // from the SDK timeline's subscribe + paginate.
    let mut relations_items: Vec<MatrixTimelineItem> = Vec::new();

    // Publish the initial snapshot from the SDK timeline.
    publish_merged_snapshot(
        handle_id, &room_id, &thread_root_id,
        &current_values, own_user_id, &empty_receipts,
        &relations_items,
        &thread_timeline_snapshot,
        &room_timeline_media_lookup,
    );

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
                            &relations_items,
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
                        // Debounce: schedule the actual /relations fetch
                        // for 1.5 seconds from now.  If more Refresh
                        // commands arrive before that, the deadline stays.
                        if refresh_deadline.is_none() {
                            refresh_deadline = Some(
                                tokio::time::Instant::now() + Duration::from_millis(1500)
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
                    Ok(items) => {
                        let prev_count = relations_items.len();
                        relations_items = items;
                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            relations_count = relations_items.len(),
                            prev_count,
                            "Refreshed thread /relations"
                        );
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
                    &relations_items,
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

/// Build a merged snapshot from SDK timeline items and /relations items,
/// then publish it to the shared snapshot and notify C++.
fn publish_merged_snapshot(
    handle_id: u64,
    room_id: &str,
    thread_root_id: &str,
    sdk_values: &Vector<Arc<TimelineItem>>,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
    read_own_event_ids: &HashSet<String>,
    relations_items: &[MatrixTimelineItem],
    thread_timeline_snapshot: &Arc<Mutex<Vec<MatrixTimelineItem>>>,
    room_timeline_media_lookup: &Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
) {
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

            // Match by event_id (available after Sent state), or
            // by sender + body content (for Pending state where
            // event_id is empty).
            let matched = if !sdk_item.event_id.is_empty() {
                relations_items.iter().find(|r| r.event_id == sdk_item.event_id)
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

// ---------------------------------------------------------------------------
// /relations fetch + raw event conversion
// ---------------------------------------------------------------------------

/// Fetch thread events from the server via `/relations` and convert them
/// to `MatrixTimelineItem` using a basic converter.  These items lack
/// SDK-processed aggregations (reactions, reply previews) but ensure
/// events delivered by sync are visible.
async fn fetch_relations_events(
    room: &Room,
    thread_root_id: &OwnedEventId,
) -> Result<Vec<MatrixTimelineItem>, String> {
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

    let mut items = Vec::with_capacity(events.len());
    for event in &events {
        if let Some(item) = raw_event_to_timeline_item(event, room, &own_user_id).await {
            items.push(item);
        }
    }

    Ok(items)
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

    let (thread_root_id, reply_to_event_id) = extract_relations_from_raw(raw.json().get());

    let is_own = deserialized.sender() == own_user_id;

    Some(MatrixTimelineItem {
        item_id: event_id.clone(),
        event_id,
        delivery_state: String::new(),
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
