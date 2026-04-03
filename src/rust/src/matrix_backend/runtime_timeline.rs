// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use super::timeline_snapshot::{build_room_timeline_snapshot, build_timeline_media_request_parameters, compute_read_own_event_ids};
use std::{
    collections::HashSet,
    sync::OnceLock,
    time::{Duration as StdDuration, Instant},
};
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
    entry.last_message_kind = newest.matrix_event_type.clone();
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
            last_message_kind: newest.matrix_event_type.clone(),
            timestamp: newest.timestamp,
        }],
    );
}

pub fn select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<(), String> {
    let room_id = room_id.trim();

    let (
        client,
        room_timeline_snapshot,
        room_timeline_media_lookup,
        previous_task,
        generation,
        initial_page_size,
        preloaded_timelines,
    ) = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get_mut(&handle_id) else {
            return Err(format!("matrix-sdk backend runtime handle {handle_id} is not active"));
        };

        if let Some(task) = handle.room_timeline_task.as_ref() {
            if task.room_id == room_id && !task.thread.is_finished() {
                tracing::debug!(
                    handle_id,
                    room_id,
                    "Matrix-sdk room timeline task is already running for the active room"
                );
                return Ok(());
            }
        }

        let previous_task = handle.room_timeline_task.take();
        let generation = handle
            .room_timeline_generation
            .fetch_add(1, Ordering::Release)
            + 1;
        handle
            .room_timeline_snapshot
            .lock()
            .expect("poisoned matrix room timeline snapshot mutex")
            .clear();
        handle
            .room_timeline_media_lookup
            .lock()
            .expect("poisoned matrix room timeline media lookup mutex")
            .clear();

        (
            handle.client.clone(),
            Arc::clone(&handle.room_timeline_snapshot),
            Arc::clone(&handle.room_timeline_media_lookup),
            previous_task,
            generation,
            handle.preferred_room_timeline_initial_page_size,
            Arc::clone(&handle.preloaded_timelines),
        )
    };

    if let Some(previous_task) = previous_task {
        std::thread::spawn(move || stop_room_timeline_task(handle_id, previous_task));
    }

    if room_id.is_empty() {
        tracing::info!(handle_id, "Cleared active matrix-sdk room timeline selection");
        return Ok(());
    }

    let stop_requested = Arc::new(AtomicBool::new(false));
    let stop_requested_for_thread = Arc::clone(&stop_requested);
    let (command_sender, command_receiver) = mpsc::unbounded_channel();
    let room_id_owned = room_id.to_owned();
    let room_id_for_thread = room_id_owned.clone();
    let generation_counter = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        handles
            .get(&handle_id)
            .map(|h| Arc::clone(&h.room_timeline_generation))
            .expect("handle must exist after select_active_room_timeline setup")
    };
    let room_timeline_task = std::thread::spawn(move || {
        crate::matrix_backend::ffi::runtime().block_on(run_room_timeline_loop(
            handle_id,
            generation,
            generation_counter,
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
            handle.room_timeline_task = Some(MatrixBackendRoomTimelineTask {
                room_id: room_id_owned.clone(),
                commands: command_sender,
                stop_requested,
                thread: room_timeline_task,
            });
        });

    tracing::info!(
        handle_id,
        room_id = %room_id_owned,
        "Started matrix-sdk room timeline task"
    );
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
        let Some(task) = handle.room_timeline_task.as_ref() else {
            return Err(format!(
                "matrix-sdk backend runtime handle {handle_id} has no active room timeline"
            ));
        };

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
        .map(|handle| {
            handle
                .room_timeline_snapshot
                .lock()
                .expect("poisoned matrix room timeline snapshot mutex")
                .clone()
        })
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;

    tracing::debug!(
        handle_id,
        item_count = snapshot.len(),
        "Fetched matrix room timeline snapshot"
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
    generation: u64,
    generation_counter: Arc<AtomicU64>,
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

    let own_user_id = client.user_id();

    // Fetch read receipt watermark targets for delivery status indicators.
    // Done once per room activation; reused for all snapshot builds.
    let receipt_targets = if let Some(room) = client.get_room(&parsed_room_id) {
        if let Some(uid) = own_user_id {
            fetch_member_receipt_targets(&room, uid).await
        } else {
            HashSet::new()
        }
    } else {
        HashSet::new()
    };

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
        // Lock the snapshot BEFORE checking the generation so that a
        // concurrent select_active_room_timeline (which bumps the generation
        // and then clears the snapshot under the same lock) cannot interleave
        // between our check and our write.  This eliminates the TOCTOU race
        // where a stale room's task could overwrite a newer room's snapshot.
        {
            let mut snapshot_guard = room_timeline_snapshot
                .lock()
                .expect("poisoned matrix room timeline snapshot mutex");
            if generation_counter.load(Ordering::Acquire) != generation {
                tracing::debug!(
                    handle_id,
                    room_id,
                    generation,
                    "Discarding stale initial matrix-sdk timeline snapshot for an inactive room generation"
                );
                return;
            }
            *snapshot_guard = snapshot;
        }
        {
            let mut media_guard = room_timeline_media_lookup
                .lock()
                .expect("poisoned matrix room timeline media lookup mutex");
            if generation_counter.load(Ordering::Acquire) != generation {
                return;
            }
            *media_guard = media_lookup;
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
                        // Lock-then-check: hold the snapshot lock while
                        // verifying the generation to prevent a stale task
                        // from overwriting a newer room's data (TOCTOU fix).
                        {
                            let mut snapshot_guard = room_timeline_snapshot
                                .lock()
                                .expect("poisoned matrix room timeline snapshot mutex");
                            if generation_counter.load(Ordering::Acquire) != generation {
                                tracing::debug!(
                                    handle_id,
                                    room_id,
                                    generation,
                                    "Stopping stale matrix-sdk room timeline loop after room switch"
                                );
                                break;
                            }
                            *snapshot_guard = snapshot;
                        }
                        {
                            let mut media_guard = room_timeline_media_lookup
                                .lock()
                                .expect("poisoned matrix room timeline media lookup mutex");
                            if generation_counter.load(Ordering::Acquire) != generation {
                                break;
                            }
                            *media_guard = media_lookup;
                        }
                        crate::ffi::matrix_notify_room_timeline_snapshot_updated(handle_id, &room_id);

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
    preloaded_timelines
        .lock()
        .expect("poisoned preloaded timelines mutex")
        .insert(room_id.clone(), timeline);

    tracing::info!(handle_id, room_id, "Matrix-sdk room timeline loop stopped");
}
