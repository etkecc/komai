// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use super::timeline_snapshot::{build_room_timeline_snapshot, build_timeline_media_request_parameters, collect_unavailable_reply_event_ids, compute_read_own_event_ids};
use std::{
    collections::HashSet,
    sync::OnceLock,
    time::{Duration as StdDuration, Instant},
};
use matrix_sdk::ruma::EventId;
use matrix_sdk_ui::timeline::TimelineReadReceiptTracking;

fn is_truthy_env_value(name: &str) -> bool {
    std::env::var_os(name).is_some_and(|value| {
        let value = value.to_string_lossy();
        !matches!(
            value.as_ref(),
            "" | "0" | "false" | "False" | "FALSE" | "no" | "No" | "NO" | "off" | "Off"
                | "OFF"
        )
    })
}

fn room_switch_perf_enabled() -> bool {
    static ENABLED: OnceLock<bool> = OnceLock::new();
    *ENABLED.get_or_init(|| {
        is_truthy_env_value("KOMAI_ROOM_SWITCH_PERF")
            || is_truthy_env_value("KOMAI_PERF_ROOM_SWITCH")
    })
}

// Debug-only knob: when set to a positive integer, on-demand backwards
// pagination of the active room timeline sleeps that many milliseconds
// before calling matrix-sdk's `paginate_backwards`.  Used to reproduce
// slow-server behaviour locally without having to find a room whose
// history is not already in the matrix-sdk cache.  No effect when unset
// or 0.  Read on every call so the value can be changed without rebuild.
fn debug_paginate_delay_ms() -> Option<u64> {
    std::env::var("KOMAI_DEBUG_PAGINATE_DELAY_MS")
        .ok()
        .and_then(|s| s.parse::<u64>().ok())
        .filter(|&ms| ms > 0)
}

fn log_room_timeline_perf(
    handle_id: u64,
    room_id: &str,
    phase: &str,
    elapsed: StdDuration,
    extra: &str,
) {
    if !room_switch_perf_enabled() {
        return;
    }

    tracing::info!(
        "[room-switch-perf] phase={} handle_id={} room_id={} elapsed_us={} elapsed_ms={:.3}{}",
        phase,
        handle_id,
        room_id,
        elapsed.as_micros(),
        elapsed.as_secs_f64() * 1000.0,
        extra
    );
}

/// After the timeline loads events for a room whose room-list entry still
/// has `timestamp == 0` (no cached latest event from sliding sync), backfill
/// the entry's data in the Rust-side room-list snapshot and notify C++ via
/// the targeted preview-update path (no model reset, preserves scroll).
///
/// This works around a matrix-sdk limitation where network backward
/// pagination does not send `RoomEventCacheGenericUpdate`, so
/// `new_latest_event` / `new_latest_event_timestamp()` remain `None`.
pub(super) fn maybe_backfill_room_list_preview(
    handle_id: u64,
    room_id: &str,
    timeline_snapshot: &[MatrixTimelineItem],
) {
    if timeline_snapshot.is_empty() {
        return;
    }

    // Find the newest event by timestamp.  Accept any event-like item
    // (including encrypted ones) — exclude only virtual items and state events.
    let is_state_like = |kind: &str| {
        matches!(
            kind,
            "other_state" | "failed_to_parse_state" | "membership_change"
        )
    };
    let Some(newest) = timeline_snapshot
        .iter()
        .filter(|item| item.timestamp > 0 && !item.event_id.is_empty() && !is_state_like(&item.item_kind))
        .max_by_key(|item| item.timestamp)
    else {
        return;
    };

    let handles = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex");
    let Some(handle) = handles.get(&handle_id) else {
        return;
    };
    let mut room_list = handle
        .room_list_snapshot
        .lock()
        .expect("poisoned matrix room-list snapshot mutex");

    let Some(entry) = room_list.iter_mut().find(|r| r.room_id == room_id) else {
        return;
    };
    if entry.timestamp > 0 {
        return; // Already has a real timestamp — nothing to backfill.
    }

    entry.timestamp = newest.timestamp;
    entry.last_message = newest.body.clone();
    entry.last_message_kind = newest.item_kind.clone();
    entry.last_message_sender_id = newest.sender_id.clone();
    entry.last_message_sender_display_name = newest.sender_display_name.clone();
    entry.latest_event_id = newest.event_id.clone();

    drop(room_list);
    drop(handles);

    tracing::info!(
        handle_id,
        room_id,
        timestamp = newest.timestamp,
        "Backfilled room-list preview from timeline"
    );

    // Targeted notification — updates the C++ model without resetting it.
    crate::ffi::matrix_notify_room_previews_backfilled(
        handle_id,
        vec![crate::ffi::MatrixRoomPreviewUpdate {
            room_id: room_id.to_owned(),
            latest_event_id: newest.event_id.clone(),
            last_message: newest.body.clone(),
            last_message_kind: newest.item_kind.clone(),
            last_message_sender_id: newest.sender_id.clone(),
            last_message_sender_display_name: newest.sender_display_name.clone(),
            timestamp: newest.timestamp,
        }],
    );
}

pub fn select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<(), String> {
    let room_id = room_id.trim();

    // Update active_room_id and check if a loop is already running for this room.
    {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get_mut(&handle_id) else {
            return Err(format!("matrix-sdk backend runtime handle {handle_id} is not active"));
        };

        if room_id.is_empty() {
            handle.active_room_id = None;
            tracing::info!(handle_id, "Cleared active matrix-sdk room timeline selection");
            return Ok(());
        }

        handle.active_room_id = Some(room_id.to_owned());

        // If a loop is already running for this room, reuse it.
        if let Some(task) = handle.room_timeline_tasks.get(room_id) {
            if !task.thread.is_finished() {
                tracing::info!(
                    handle_id,
                    room_id,
                    task_count = handle.room_timeline_tasks.len(),
                    "Matrix-sdk room timeline task is already running, reusing"
                );
                return Ok(());
            }
            // Loop finished (crashed?) — remove and restart below.
            tracing::warn!(
                handle_id,
                room_id,
                "Matrix-sdk room timeline task had finished, restarting"
            );
        }
    }

    // Start a new loop for this room.
    let (
        client,
        room_timeline_snapshot,
        room_timeline_media_lookup,
        initial_page_size,
        preloaded_timelines,
        thread_reply_counts,
    ) = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles
            .get_mut(&handle_id)
            .expect("handle must exist after active_room_id update");

        // Remove any finished task entry for this room.
        handle.room_timeline_tasks.remove(room_id);

        // Get or create per-room snapshot.
        let room_timeline_snapshot = handle
            .room_timeline_snapshots
            .entry(room_id.to_owned())
            .or_insert_with(|| Arc::new(Mutex::new(Vec::new())));
        // Clear stale snapshot data for a fresh loop.
        room_timeline_snapshot
            .lock()
            .expect("poisoned matrix room timeline snapshot mutex")
            .clear();

        (
            handle.client.clone(),
            Arc::clone(room_timeline_snapshot),
            Arc::clone(&handle.room_timeline_media_lookup),
            handle.preferred_room_timeline_initial_page_size,
            Arc::clone(&handle.preloaded_timelines),
            Arc::clone(&handle.thread_reply_counts),
        )
    };

    let stop_requested = Arc::new(AtomicBool::new(false));
    let stop_requested_for_thread = Arc::clone(&stop_requested);
    let (command_sender, command_receiver) = mpsc::unbounded_channel();
    let room_id_owned = room_id.to_owned();
    let room_id_for_thread = room_id_owned.clone();
    let room_timeline_task = std::thread::spawn(move || {
        crate::matrix_backend::ffi::runtime().block_on(run_room_timeline_loop(
            handle_id,
            client,
            room_id_for_thread,
            initial_page_size,
            room_timeline_snapshot,
            room_timeline_media_lookup,
            command_receiver,
            stop_requested_for_thread,
            preloaded_timelines,
            thread_reply_counts,
        ));
    });

    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .entry(handle_id)
        .and_modify(|handle| {
            handle.room_timeline_tasks.insert(
                room_id_owned.clone(),
                MatrixBackendRoomTimelineTask {
                    room_id: room_id_owned.clone(),
                    commands: command_sender,
                    stop_requested,
                    thread: room_timeline_task,
                },
            );
        });

    tracing::info!(
        handle_id,
        room_id = %room_id_owned,
        "Started matrix-sdk room timeline task"
    );
    Ok(())
}

/// Stop a specific room's timeline loop and remove it from the task map.
pub fn stop_room_timeline(handle_id: u64, room_id: &str) -> Result<(), String> {
    let room_id = room_id.trim();
    let task = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles
            .get_mut(&handle_id)
            .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;
        handle.room_timeline_tasks.remove(room_id)
    };

    if let Some(task) = task {
        std::thread::spawn(move || stop_room_timeline_task(handle_id, task));
        tracing::info!(handle_id, room_id, "Stopping matrix-sdk room timeline task");
    }

    // Clean up the per-room snapshot.
    {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        if let Some(handle) = handles.get_mut(&handle_id) {
            handle.room_timeline_snapshots.remove(room_id);
        }
    }

    // Drop any cached thread timelines tied to this room.  The room is
    // leaving the cache (LRU eviction at the C++ per-room model layer), so
    // its threads cannot be served from the warm cache anyway — keeping
    // them around would pin SDK Timeline state for a room we've decided to
    // forget about.
    super::thread_timeline::evict_thread_subscriptions_for_room(handle_id, room_id);

    Ok(())
}

pub fn set_active_room_timeline_initial_page_size(
    handle_id: u64,
    page_size: u16,
) -> Result<(), String> {
    let mut handles = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex");
    let Some(handle) = handles.get_mut(&handle_id) else {
        return Err(format!("matrix-sdk backend runtime handle {handle_id} is not active"));
    };

    let clamped_page_size = page_size.clamp(ROOM_TIMELINE_INITIAL_PAGE_SIZE, ROOM_TIMELINE_PAGE_SIZE);
    handle.preferred_room_timeline_initial_page_size = clamped_page_size;
    Ok(())
}

pub fn paginate_active_room_timeline_backwards(
    handle_id: u64,
    page_size: u16,
) -> Result<(), String> {
    let (room_id, command_sender) = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get(&handle_id) else {
            return Err(format!("matrix-sdk backend runtime handle {handle_id} is not active"));
        };
        let active_room = handle.active_room_id.as_ref().ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} has no active room")
        })?;
        let task = handle.room_timeline_tasks.get(active_room).ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no timeline task for active room '{active_room}'"
            )
        })?;

        if task.thread.is_finished() {
            return Err(format!(
                "matrix-sdk active room timeline task for '{}' is no longer running",
                task.room_id
            ));
        }

        (task.room_id.clone(), task.commands.clone())
    };

    let page_size = if page_size == 0 {
        ROOM_TIMELINE_PAGE_SIZE
    } else {
        page_size
    };

    command_sender
        .send(MatrixBackendRoomTimelineCommand::PaginateBackwards(page_size))
        .map_err(|_| {
            format!(
                "failed to send pagination command to active matrix-sdk room timeline '{}'",
                room_id
            )
        })?;

    tracing::debug!(
        handle_id,
        room_id,
        page_size,
        "Queued matrix-sdk room timeline backwards pagination"
    );

    Ok(())
}

pub async fn fetch_active_room_timeline(handle_id: u64) -> Result<Vec<MatrixTimelineItem>, String> {
    let snapshot = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .and_then(|handle| {
            let room_id = handle.active_room_id.as_ref()?;
            let snapshot_arc = handle.room_timeline_snapshots.get(room_id)?;
            Some(
                snapshot_arc
                    .lock()
                    .expect("poisoned matrix room timeline snapshot mutex")
                    .clone(),
            )
        })
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active or has no active room"))?;

    tracing::debug!(
        handle_id,
        item_count = snapshot.len(),
        "Fetched active matrix room timeline snapshot"
    );

    Ok(snapshot)
}

/// Fetch a specific room's timeline snapshot (for per-room model updates).
pub async fn fetch_room_timeline_snapshot(
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<MatrixTimelineItem>, String> {
    let snapshot = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .and_then(|handle| {
            let snapshot_arc = handle.room_timeline_snapshots.get(room_id)?;
            Some(
                snapshot_arc
                    .lock()
                    .expect("poisoned matrix room timeline snapshot mutex")
                    .clone(),
            )
        })
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no snapshot for room '{room_id}'"
            )
        })?;

    tracing::debug!(
        handle_id,
        room_id,
        item_count = snapshot.len(),
        "Fetched room timeline snapshot"
    );

    Ok(snapshot)
}

/// Build a live room timeline with read-receipt and fully-read-marker
/// tracking enabled.
///
/// Tracking is scoped to message-like events (matching the SDK's default
/// event filter, which only materializes those into items) — this is what
/// makes the timeline stream emit `Set` diffs when another member's read
/// receipt moves, keeping the "Received" → "Read" delivery indicator live
/// without polling.  Every path that builds a room timeline (the active
/// loop, the background preloader, the one-shot IPC fetch) goes through here
/// so the cached `Timeline` handles all share the same tracking config.
pub(crate) async fn build_room_timeline(
    room: &matrix_sdk::Room,
) -> Result<Timeline, matrix_sdk_ui::timeline::Error> {
    room.timeline_builder()
        .track_read_marker_and_receipts(TimelineReadReceiptTracking::MessageLikeEvents)
        .build()
        .await
}

/// Fetch a room's timeline with optional server-side backfill.
///
/// Uses the preloaded timeline cache when available, otherwise builds a fresh
/// timeline handle.  If the cached items are fewer than `limit`, paginates
/// backwards to fetch more from the server.  The timeline handle is always
/// returned to the cache for reuse.
pub async fn fetch_room_timeline(
    handle_id: u64,
    room_id: &str,
    limit: u16,
) -> Result<Vec<MatrixTimelineItem>, String> {
    let (client, preloaded_timelines) = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles
            .get(&handle_id)
            .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;
        (handle.client.clone(), Arc::clone(&handle.preloaded_timelines))
    };

    let cached = preloaded_timelines
        .lock()
        .expect("poisoned preloaded timelines mutex")
        .remove(room_id);

    let timeline = if let Some(t) = cached {
        t
    } else {
        let parsed_room_id = RoomId::parse(room_id)
            .map_err(|e| format!("invalid room id: {e}"))?;
        let room = client
            .get_room(&parsed_room_id)
            .ok_or_else(|| format!("room '{}' not known to client", room_id))?;
        build_room_timeline(&room)
            .await
            .map_err(|e| format!("failed to build timeline for '{}': {e}", room_id))?
    };

    let (items, _stream) = timeline.subscribe().await;

    // Paginate backwards if we don't have enough items.
    if items.len() < limit as usize {
        if let Err(e) = timeline.paginate_backwards(limit).await {
            tracing::warn!(
                handle_id,
                room_id,
                "IPC timeline backfill pagination failed: {e}"
            );
        }
    }

    let (items_after, _stream) = timeline.subscribe().await;
    let items_to_convert = if items_after.len() >= items.len() {
        &items_after
    } else {
        &items
    };

    let empty_receipts = HashSet::new();
    let (snapshot, _media_lookup) =
        build_room_timeline_snapshot(items_to_convert, None, &empty_receipts, None);

    // Cache the timeline handle back for reuse.
    preloaded_timelines
        .lock()
        .expect("poisoned preloaded timelines mutex")
        .insert(room_id.to_owned(), timeline);

    tracing::debug!(
        handle_id,
        room_id,
        item_count = snapshot.len(),
        "Fetched room timeline with backfill"
    );

    Ok(snapshot)
}

pub async fn fetch_active_room_timeline_media_content(
    handle_id: u64,
    item_id: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    let client = client_for_handle(handle_id)?;
    let item_id = item_id.trim();
    if item_id.is_empty() {
        return Err("cannot fetch matrix-sdk timeline media without an item id".to_owned());
    }

    let media_request = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .and_then(|handle| {
            handle
                .room_timeline_media_lookup
                .lock()
                .expect("poisoned matrix room timeline media lookup mutex")
                .get(item_id)
                .cloned()
        })
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no active timeline media for item '{item_id}'"
            )
        })?;

    let request =
        build_timeline_media_request_parameters(&media_request, width, height, crop)?;

    tracing::debug!(
        handle_id,
        item_id,
        width,
        height,
        crop,
        "Fetching matrix-sdk active timeline media content"
    );

    // use_cache = true: persist the fetched media in the SDK store so repeat
    // requests for the same item (the media overlay decodes it at several sizes,
    // plus re-opens and gallery revisits) are served locally instead of
    // re-downloading the full file from the homeserver every time.
    client
        .media()
        .get_media_content(&request, true)
        .await
        .map_err(|e| format!("failed to fetch matrix-sdk active timeline media: {e}"))
}

/// Client-side TTL for stuck typing indicators.  matrix-sdk's
/// `subscribe_to_typing_notifications` is purely event-driven: if the
/// server's "stopped typing" `m.typing` ephemeral is dropped (federation
/// EDU loss, sync gap, broadcast channel lag) the indicator hangs
/// forever.  See etkecc/komai#117.
///
/// 15s is ~4× the server-side typing timeout (matrix-sdk PUTs
/// `typing=true` with a 4s timeout while the composer is active).  Any
/// composer pause ≥4s expires the server's state and produces a fresh
/// `m.typing` event when the user resumes, refreshing the per-user
/// TTL clock; real-world typing rarely sustains 15s of continuous
/// keystrokes with zero such pauses.
pub(super) const TYPING_USER_TTL: StdDuration = StdDuration::from_secs(15);

/// Minimum interval between two TTL prunes.  Pruning happens inside
/// the existing 50ms stop-poll arm; this gates how often we actually
/// rescan `typing_state`.
pub(super) const TYPING_PRUNE_INTERVAL: StdDuration = StdDuration::from_secs(2);

pub(super) struct RoomTypingEntry {
    display_name: String,
    first_seen_at: Instant,
    last_seen_at: Instant,
}

/// Compare the current typing state against the last list we published
/// to C++; emit a new FFI notification only when the rendered list
/// actually changes.  Sorts by `first_seen_at` so the UI shows the
/// oldest typer first — matrix-sdk's broadcast carries whatever order
/// the server's `m.typing` happened to use, which is unstable across
/// events and looks janky.
pub(super) fn publish_typing_state_if_changed(
    handle_id: u64,
    room_id: &str,
    typing_state: &HashMap<OwnedUserId, RoomTypingEntry>,
    last_published: &mut Vec<String>,
) {
    let mut entries: Vec<(&OwnedUserId, &RoomTypingEntry)> = typing_state.iter().collect();
    entries.sort_by(|(a_uid, a), (b_uid, b)| {
        a.first_seen_at
            .cmp(&b.first_seen_at)
            .then_with(|| a_uid.as_str().cmp(b_uid.as_str()))
    });
    let names: Vec<String> = entries
        .into_iter()
        .map(|(_, e)| e.display_name.clone())
        .collect();
    if names != *last_published {
        *last_published = names.clone();
        crate::ffi::matrix_notify_typing_users_updated(handle_id, room_id, names);
    }
}

mod task_loop;

use task_loop::run_room_timeline_loop;
