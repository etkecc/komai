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
use matrix_sdk::ruma::events::receipt::{ReceiptThread, ReceiptType};

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
fn maybe_backfill_room_list_preview(
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
        room.timeline()
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
        build_room_timeline_snapshot(items_to_convert, None, &empty_receipts);

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

    client
        .media()
        .get_media_content(&request, false)
        .await
        .map_err(|e| format!("failed to fetch matrix-sdk active timeline media: {e}"))
}

/// Fetch the event IDs that non-own members' latest read receipts point to.
/// These are used as watermark targets: every own event at or before the
/// newest target in the timeline is considered "read".
async fn fetch_member_receipt_targets(
    room: &matrix_sdk::Room,
    own_user_id: &matrix_sdk::ruma::UserId,
) -> HashSet<String> {
    let members = match room.members(matrix_sdk::RoomMemberships::ACTIVE).await {
        Ok(m) => m,
        Err(e) => {
            tracing::warn!("Failed to fetch room members for receipt watermark: {e}");
            return HashSet::new();
        }
    };

    let mut targets = HashSet::new();
    for member in members.iter() {
        if member.user_id() == own_user_id {
            continue;
        }
        // Try Main thread (newer spec) first, then Unthreaded (legacy).
        for thread in [ReceiptThread::Main, ReceiptThread::Unthreaded] {
            if let Ok(Some((event_id, _))) =
                room.load_user_receipt(ReceiptType::Read, thread, member.user_id()).await
            {
                targets.insert(event_id.to_string());
                break;
            }
        }
    }

    targets
}

async fn run_room_timeline_loop(
    handle_id: u64,
    client: Client,
    room_id: String,
    initial_page_size: u16,
    room_timeline_snapshot: Arc<Mutex<Vec<MatrixTimelineItem>>>,
    room_timeline_media_lookup: Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
    mut commands: mpsc::UnboundedReceiver<MatrixBackendRoomTimelineCommand>,
    stop_requested: Arc<AtomicBool>,
    preloaded_timelines: Arc<Mutex<HashMap<String, Timeline>>>,
) {
    tracing::info!(handle_id, room_id, "Running matrix-sdk room timeline loop");
    let loop_started_at = Instant::now();

    let parsed_room_id = match RoomId::parse(&room_id) {
        Ok(room_id) => room_id,
        Err(error) => {
            tracing::warn!(handle_id, room_id, %error, "Invalid room id for room timeline task");
            return;
        }
    };

    // Try to reuse a cached timeline handle from the background preloader
    // or a previous room view.
    //
    // In matrix-sdk 0.16, `room.timeline().await` + `timeline.subscribe().await`
    // takes ~2-3 seconds per room, even when events are already in the local
    // SQLite event cache.  The cost comes from rebuilding the in-memory timeline
    // state (deserializing cached events, setting up the chunk structure, etc.).
    // Keeping the Timeline handle alive avoids this rebuild entirely — the
    // internal state is already populated and `subscribe()` returns immediately.
    //
    // The background preloader (runtime_preloader.rs) eagerly builds and caches
    // Timeline handles for all rooms after initial sync.  When the user switches
    // away from a room, the handle is also cached back (see end of this loop).
    let cached_timeline = preloaded_timelines
        .lock()
        .expect("poisoned preloaded timelines mutex")
        .remove(&room_id);

    let timeline = if let Some(cached) = cached_timeline {
        log_room_timeline_perf(
            handle_id,
            &room_id,
            "rust.matrix_timeline.timeline_from_cache",
            loop_started_at.elapsed(),
            "",
        );
        cached
    } else {
        let get_room_started_at = Instant::now();
        let Some(room) = client.get_room(&parsed_room_id) else {
            tracing::warn!(
                handle_id,
                room_id,
                "Matrix-sdk client does not know the requested room"
            );
            return;
        };
        log_room_timeline_perf(
            handle_id,
            &room_id,
            "rust.matrix_timeline.get_room",
            get_room_started_at.elapsed(),
            "",
        );

        let build_started_at = Instant::now();
        match room.timeline().await {
            Ok(timeline) => {
                log_room_timeline_perf(
                    handle_id,
                    &room_id,
                    "rust.matrix_timeline.timeline_build",
                    build_started_at.elapsed(),
                    "",
                );
                timeline
            }
            Err(error) => {
                tracing::warn!(
                    handle_id,
                    room_id,
                    %error,
                    "Failed to build matrix-sdk timeline"
                );
                return;
            }
        }
    };
    let timeline = Arc::new(timeline);

    let own_user_id = client.user_id();

    // Subscribe to typing notifications for this room.
    // The EventHandlerDropGuard auto-unsubscribes when this loop exits.
    let (_typing_drop_guard, mut typing_receiver) = {
        let room = client.get_room(&parsed_room_id);
        match room {
            Some(room) => {
                let (guard, receiver) = room.subscribe_to_typing_notifications();
                (Some(guard), Some(receiver))
            }
            None => (None, None),
        }
    };

    // Receipt targets are fetched after the initial snapshot so they
    // don't block the first paint.  Start with an empty set — delivery
    // status indicators will appear once receipts are loaded.
    let mut receipt_targets = HashSet::new();

    let subscribe_started_at = Instant::now();
    let (items, stream) = timeline.subscribe().await;
    log_room_timeline_perf(
        handle_id,
        &room_id,
        "rust.matrix_timeline.subscribe",
        subscribe_started_at.elapsed(),
        &format!(" subscribe_count={}", items.len()),
    );
    let mut current_values = items;
    let subscribe_count = current_values.len();
    {
        let read_own_event_ids = compute_read_own_event_ids(&current_values, &receipt_targets);
        let snapshot_build_started_at = Instant::now();
        let (snapshot, media_lookup) = build_room_timeline_snapshot(&current_values, own_user_id, &read_own_event_ids);
        let snapshot_build_elapsed = snapshot_build_started_at.elapsed();
        let snapshot_count = snapshot.len();
        {
            let mut snapshot_guard = room_timeline_snapshot
                .lock()
                .expect("poisoned matrix room timeline snapshot mutex");
            *snapshot_guard = snapshot;
        }
        {
            let mut media_guard = room_timeline_media_lookup
                .lock()
                .expect("poisoned matrix room timeline media lookup mutex");
            media_guard.extend(media_lookup);
        }
        tracing::info!(
            handle_id,
            room_id,
            subscribe_count,
            snapshot_count,
            "Initial matrix-sdk timeline subscribe"
        );
        log_room_timeline_perf(
            handle_id,
            &room_id,
            "rust.matrix_timeline.initial_snapshot_build",
            snapshot_build_elapsed,
            &format!(
                " subscribe_count={} snapshot_count={}",
                subscribe_count, snapshot_count
            ),
        );
        // Only notify C++ when there are actual items.  For rooms with
        // no cached events the initial subscribe returns 0 items; sending
        // an empty notification would clear the loading flag prematurely
        // (showing "Nothing has loaded" while pagination is still running).
        if snapshot_count > 0 {
            crate::ffi::matrix_notify_room_timeline_snapshot_updated(handle_id, &room_id);
            log_room_timeline_perf(
                handle_id,
                &room_id,
                "rust.matrix_timeline.initial_snapshot_notified",
                loop_started_at.elapsed(),
                &format!(
                    " subscribe_count={} snapshot_count={}",
                    subscribe_count, snapshot_count
                ),
            );
        }
    }

    // Fetch reply details for events whose replied-to content is unavailable.
    // These fire-and-forget tasks update the timeline items via diffs.
    let mut reply_detail_fetch_requested: HashSet<OwnedEventId> = HashSet::new();
    for event_id in collect_unavailable_reply_event_ids(&current_values) {
        if reply_detail_fetch_requested.insert(event_id.clone()) {
            let timeline_clone = timeline.clone();
            tokio::spawn(async move {
                if let Err(e) = timeline_clone.fetch_details_for_event(&event_id).await {
                    tracing::info!("Failed to fetch reply details for {}: {e}", event_id);
                }
            });
        }
    }

    // Fetch read receipt watermark targets now that the initial snapshot
    // has been delivered.  Delivery status indicators update shortly after.
    receipt_targets = if let Some(room) = client.get_room(&parsed_room_id) {
        if let Some(uid) = own_user_id {
            fetch_member_receipt_targets(&room, uid).await
        } else {
            HashSet::new()
        }
    } else {
        HashSet::new()
    };
    if !receipt_targets.is_empty() && subscribe_count > 0 {
        let read_own_event_ids = compute_read_own_event_ids(&current_values, &receipt_targets);
        let (snapshot, media_lookup) =
            build_room_timeline_snapshot(&current_values, own_user_id, &read_own_event_ids);
        {
            let mut snapshot_guard = room_timeline_snapshot
                .lock()
                .expect("poisoned matrix room timeline snapshot mutex");
            *snapshot_guard = snapshot;
        }
        {
            let mut media_guard = room_timeline_media_lookup
                .lock()
                .expect("poisoned matrix room timeline media lookup mutex");
            media_guard.extend(media_lookup);
        }
        crate::ffi::matrix_notify_room_timeline_snapshot_updated(handle_id, &room_id);
    }

    tracing::info!(
        handle_id,
        room_id,
        initial_page_size,
        "Requesting initial backwards pagination"
    );
    let paginate_started_at = Instant::now();
    // Scope the initial paginate future so its borrow on `timeline` is
    // released before we cache the handle at the end.
    {
    let initial_paginate_fut = timeline.paginate_backwards(initial_page_size);
    tokio::pin!(initial_paginate_fut);
    let mut initial_paginate_pending = true;

    let mut stream = Box::pin(stream);
    let mut first_diff_logged = false;
    while !stop_requested.load(Ordering::Relaxed) {
        tokio::select! {
            result = &mut initial_paginate_fut, if initial_paginate_pending => {
                initial_paginate_pending = false;
                if let Err(error) = result {
                    tracing::warn!(
                        handle_id,
                        room_id,
                        %error,
                        "Initial matrix-sdk room timeline pagination failed"
                    );
                }
                log_room_timeline_perf(
                    handle_id,
                    &room_id,
                    "rust.matrix_timeline.initial_paginate",
                    paginate_started_at.elapsed(),
                    &format!(" page_size={}", initial_page_size),
                );
            }

            maybe_diffs = stream.next() => {
                match maybe_diffs {
                    Some(diffs) => {
                        let diffs: Vec<VectorDiff<Arc<TimelineItem>>> = diffs;
                        let first_diff_started_at = Instant::now();
                        for diff in diffs.iter().cloned() {
                            diff.apply(&mut current_values);
                        }

                        let read_own_event_ids = compute_read_own_event_ids(&current_values, &receipt_targets);
                        let snapshot_build_started_at = Instant::now();
                        let (snapshot, media_lookup) =
                            build_room_timeline_snapshot(&current_values, own_user_id, &read_own_event_ids);
                        let snapshot_build_elapsed = snapshot_build_started_at.elapsed();
                        let item_count = snapshot.len();
                        {
                            let mut snapshot_guard = room_timeline_snapshot
                                .lock()
                                .expect("poisoned matrix room timeline snapshot mutex");
                            *snapshot_guard = snapshot;
                        }
                        {
                            let mut media_guard = room_timeline_media_lookup
                                .lock()
                                .expect("poisoned matrix room timeline media lookup mutex");
                            media_guard.extend(media_lookup);
                        }
                        crate::ffi::matrix_notify_room_timeline_snapshot_updated(handle_id, &room_id);

                        for event_id in collect_unavailable_reply_event_ids(&current_values) {
                            if reply_detail_fetch_requested.insert(event_id.clone()) {
                                let timeline_clone = timeline.clone();
                                tokio::spawn(async move {
                                    if let Err(e) = timeline_clone.fetch_details_for_event(&event_id).await {
                                        tracing::info!("Failed to fetch reply details for {}: {e}", event_id);
                                    }
                                });
                            }
                        }

                        let diff_count = diffs.len();
                        tracing::info!(
                            handle_id,
                            room_id,
                            item_count,
                            diff_count,
                            "Updated matrix-sdk room timeline snapshot"
                        );
                        if !first_diff_logged {
                            first_diff_logged = true;
                            log_room_timeline_perf(
                                handle_id,
                                &room_id,
                                "rust.matrix_timeline.first_diff_applied",
                                first_diff_started_at.elapsed(),
                                &format!(
                                    " diff_count={} current_value_count={} snapshot_count={}",
                                    diff_count,
                                    current_values.len(),
                                    item_count
                                ),
                            );
                            log_room_timeline_perf(
                                handle_id,
                                &room_id,
                                "rust.matrix_timeline.first_stream_snapshot_build",
                                snapshot_build_elapsed,
                                &format!(
                                    " diff_count={} current_value_count={} snapshot_count={}",
                                    diff_count,
                                    current_values.len(),
                                    item_count
                                ),
                            );
                            log_room_timeline_perf(
                                handle_id,
                                &room_id,
                                "rust.matrix_timeline.first_stream_snapshot_notified",
                                loop_started_at.elapsed(),
                                &format!(
                                    " diff_count={} current_value_count={} snapshot_count={}",
                                    diff_count,
                                    current_values.len(),
                                    item_count
                                ),
                            );

                            // Backfill the room-list entry if it still lacks a
                            // timestamp (sliding sync didn't deliver a latest
                            // event for this room).
                            {
                                let snap = room_timeline_snapshot
                                    .lock()
                                    .expect("poisoned matrix room timeline snapshot mutex");
                                maybe_backfill_room_list_preview(handle_id, &room_id, &snap);
                            }
                        }
                    }
                    None => {
                        tracing::info!(handle_id, room_id, "Matrix-sdk room timeline stream ended");
                        break;
                    }
                }
            }

            maybe_command = commands.recv() => {
                match maybe_command {
                    Some(MatrixBackendRoomTimelineCommand::PaginateBackwards(page_size)) => {
                        tracing::info!(
                            handle_id,
                            room_id,
                            page_size,
                            "Paginating active matrix-sdk room timeline backwards"
                        );
                        if let Err(error) = timeline.paginate_backwards(page_size).await {
                            tracing::warn!(
                                handle_id,
                                room_id,
                                page_size,
                                %error,
                                "Failed to paginate active matrix-sdk room timeline backwards"
                            );
                            // Re-notify so the C++ side processes the current snapshot
                            // instead of waiting for a larger one that will never arrive.
                            crate::ffi::matrix_notify_room_timeline_snapshot_updated(
                                handle_id,
                                &room_id,
                            );
                        }
                    }
                    Some(MatrixBackendRoomTimelineCommand::ToggleReaction { event_id, reaction_key, response }) => {
                        tracing::info!(
                            handle_id,
                            room_id,
                            event_id,
                            reaction_key,
                            "Toggling reaction on active matrix-sdk room timeline"
                        );
                        let result = match EventId::parse(&event_id) {
                            Ok(parsed_event_id) => {
                                timeline
                                    .toggle_reaction(&TimelineEventItemId::EventId(parsed_event_id), &reaction_key)
                                    .await
                                    .map(|_| ())
                                    .map_err(|e| format!("failed to toggle matrix-sdk room reaction: {e}"))
                            }
                            Err(e) => Err(format!("invalid event id '{event_id}': {e}")),
                        };
                        let _ = response.send(result);
                    }
                    None => {
                        tracing::debug!(
                            handle_id,
                            room_id,
                            "Matrix-sdk room timeline command channel closed"
                        );
                        break;
                    }
                }
            }

            maybe_typing = async {
                match typing_receiver.as_mut() {
                    Some(rx) => rx.recv().await,
                    None => std::future::pending().await,
                }
            } => {
                match maybe_typing {
                    Ok(typing_user_ids) => {
                        let mut display_names: Vec<String> = Vec::with_capacity(typing_user_ids.len());
                        if let Some(room) = client.get_room(&parsed_room_id) {
                            for user_id in &typing_user_ids {
                                let name = match room.get_member_no_sync(user_id).await {
                                    Ok(Some(member)) => member
                                        .display_name()
                                        .unwrap_or_else(|| user_id.as_str())
                                        .to_owned(),
                                    _ => user_id.to_string(),
                                };
                                display_names.push(name);
                            }
                        } else {
                            for user_id in &typing_user_ids {
                                display_names.push(user_id.to_string());
                            }
                        }
                        crate::ffi::matrix_notify_typing_users_updated(
                            handle_id,
                            &room_id,
                            display_names,
                        );
                    }
                    Err(tokio::sync::broadcast::error::RecvError::Lagged(_)) => {
                        // Missed some events; continue to get latest state.
                    }
                    Err(tokio::sync::broadcast::error::RecvError::Closed) => {
                        typing_receiver = None;
                    }
                }
            }

            _ = tokio::time::sleep(Duration::from_millis(ROOM_TIMELINE_STOP_POLL_INTERVAL_MS)) => {
                if stop_requested.load(Ordering::Relaxed) {
                    break;
                }
            }
        }
    }
    } // Drop initial_paginate_fut, releasing borrow on timeline

    // Cache the timeline handle back so switching to this room again
    // doesn't require the expensive ~2-3s room.timeline() + subscribe()
    // rebuild.  See the comment at the top of this function for context.
    match Arc::try_unwrap(timeline) {
        Ok(timeline) => {
            preloaded_timelines
                .lock()
                .expect("poisoned preloaded timelines mutex")
                .insert(room_id.clone(), timeline);
        }
        Err(_) => {
            tracing::warn!(
                handle_id,
                room_id,
                "Cannot cache timeline handle back — reply detail fetch tasks still hold a reference"
            );
        }
    }

    tracing::info!(handle_id, room_id, "Matrix-sdk room timeline loop stopped");
}
