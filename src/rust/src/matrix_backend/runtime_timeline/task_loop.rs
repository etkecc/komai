// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! The async event loop driving a single room timeline subscription:
//! drains SDK VectorDiff updates, polls the command channel, manages
//! typing state, retries / cancels local echo, and publishes snapshots.

use super::*;


pub(super) async fn run_room_timeline_loop(
    handle_id: u64,
    client: Client,
    room_id: String,
    initial_page_size: u16,
    room_timeline_snapshot: Arc<Mutex<Vec<MatrixTimelineItem>>>,
    room_timeline_media_lookup: Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
    mut commands: mpsc::UnboundedReceiver<MatrixBackendRoomTimelineCommand>,
    stop_requested: Arc<AtomicBool>,
    preloaded_timelines: Arc<Mutex<HashMap<String, Timeline>>>,
    thread_reply_counts: Arc<Mutex<HashMap<String, HashMap<String, u32>>>>,
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
        match build_room_timeline(&room).await {
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

    // Per-user typing state (see TYPING_USER_TTL above).  Replaced
    // wholesale on every `m.typing` event; entries that aren't refreshed
    // within TYPING_USER_TTL are pruned by the 50ms stop-poll arm.
    let mut typing_state: HashMap<OwnedUserId, RoomTypingEntry> = HashMap::new();
    let mut last_published_typing: Vec<String> = Vec::new();
    let mut last_typing_prune = Instant::now();

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

    // Build the merged thread-reply-count map for a snapshot: start from the
    // local-derived counts (scan of `current_values`, gives accurate counts for
    // threads whose replies are all loaded), then overlay the server-side cache
    // (populated by `list_threads()`) using max() so authoritative server
    // counts win for threads with replies in scrollback we haven't paginated.
    //
    // Computing local counts fresh on every snapshot (instead of incrementing
    // a long-lived cache) avoids double-counting when pagination loads replies
    // already accounted for by `list_threads()`, which previously made the
    // thread-root badge show e.g. 12 for a thread that actually has 6 replies.
    let merged_thread_counts = |values: &Vector<Arc<TimelineItem>>, cache: &Arc<Mutex<HashMap<String, HashMap<String, u32>>>>, rid: &str| -> HashMap<String, u32> {
        let mut counts: HashMap<String, u32> = HashMap::new();
        let mut seen: HashSet<String> = HashSet::new();
        for item in values.iter() {
            if let Some(event) = item.as_event() {
                if let Some(event_id) = event.event_id() {
                    if let matrix_sdk_ui::timeline::TimelineItemContent::MsgLike(msg) = event.content() {
                        if let Some(thread_root) = &msg.thread_root {
                            if seen.insert(event_id.to_string()) {
                                *counts.entry(thread_root.to_string()).or_insert(0) += 1;
                            }
                        }
                    }
                }
            }
        }
        let guard = cache.lock().expect("poisoned thread reply counts mutex");
        if let Some(server_counts) = guard.get(rid) {
            for (root_id, &server_count) in server_counts {
                counts
                    .entry(root_id.clone())
                    .and_modify(|c| *c = (*c).max(server_count))
                    .or_insert(server_count);
            }
        }
        counts
    };

    {
        let read_own_event_ids = compute_read_own_event_ids(&current_values, own_user_id);
        let snapshot_build_started_at = Instant::now();
        let room_counts = merged_thread_counts(&current_values, &thread_reply_counts, &room_id);
        let (snapshot, media_lookup) = build_room_timeline_snapshot(&current_values, own_user_id, &read_own_event_ids, Some(&room_counts));
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

    // Spawn a background task to fetch thread reply counts from the server.
    // This populates the shared cache so subsequent snapshot builds include
    // accurate counts on thread root messages.  We deliberately do NOT
    // rebuild the snapshot here: the main loop owns `current_values` and
    // a clone captured at spawn time becomes stale once backward pagination
    // diffs arrive, so writing it back would clobber the live snapshot
    // (e.g. revert 38 paginated items back to the 20 we had at subscribe).
    // The next diff in the main loop reads the cache via `merged_thread_counts`
    // and rebuilds with accurate counts naturally.
    {
        let client_clone = client.clone();
        let room_id_clone = room_id.clone();
        let cache_clone = Arc::clone(&thread_reply_counts);
        tokio::spawn(async move {
            if let Some(room) = client_clone.get_room(
                &matrix_sdk::ruma::RoomId::parse(&room_id_clone).expect("already validated room_id"),
            ) {
                let opts = matrix_sdk::room::ListThreadsOptions::default();
                match room.list_threads(opts).await {
                    Ok(result) => {
                        let mut counts = HashMap::new();
                        for event in &result.chunk {
                            if let Some(event_id) = event.event_id() {
                                let reply_count = match &event.thread_summary {
                                    matrix_sdk_base::deserialized_responses::ThreadSummaryStatus::Some(summary) => summary.num_replies,
                                    _ => 0,
                                };
                                if reply_count > 0 {
                                    counts.insert(event_id.to_string(), reply_count);
                                }
                            }
                        }
                        if !counts.is_empty() {
                            let thread_count = counts.len();
                            {
                                // Overwrite the room's authoritative server-side
                                // count map.  Local-derived counts are computed
                                // separately at snapshot-build time via
                                // `merged_thread_counts`, so this cache only ever
                                // stores what `list_threads()` last returned.
                                let mut guard = cache_clone.lock().expect("poisoned thread reply counts mutex");
                                guard.insert(room_id_clone.clone(), counts);
                            }
                            tracing::info!(handle_id, room_id = %room_id_clone, thread_count, "Cached thread reply counts from list_threads()");
                        }
                    }
                    Err(error) => {
                        tracing::warn!(handle_id, room_id = %room_id_clone, %error, "Failed to fetch thread list for reply counts");
                    }
                }
            }
        });
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

                        let read_own_event_ids = compute_read_own_event_ids(&current_values, own_user_id);
                        let snapshot_build_started_at = Instant::now();
                        let room_counts = merged_thread_counts(&current_values, &thread_reply_counts, &room_id);
                        let (snapshot, media_lookup) =
                            build_room_timeline_snapshot(&current_values, own_user_id, &read_own_event_ids, Some(&room_counts));
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
                        crate::ffi::matrix_notify_room_timeline_pagination_state(
                            handle_id,
                            &room_id,
                            true,
                        );
                        if let Some(delay_ms) = debug_paginate_delay_ms() {
                            tracing::warn!(
                                handle_id,
                                room_id,
                                delay_ms,
                                "KOMAI_DEBUG_PAGINATE_DELAY_MS active; sleeping before paginate_backwards"
                            );
                            tokio::time::sleep(StdDuration::from_millis(delay_ms)).await;
                        }
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
                        crate::ffi::matrix_notify_room_timeline_pagination_state(
                            handle_id,
                            &room_id,
                            false,
                        );
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
                    Some(MatrixBackendRoomTimelineCommand::CancelLocalEcho { transaction_id, response }) => {
                        tracing::info!(
                            handle_id,
                            room_id,
                            transaction_id,
                            "Cancelling local echo on active matrix-sdk room timeline"
                        );
                        let result = cancel_local_echo_for_timeline(&timeline, &transaction_id).await;
                        let _ = response.send(result);
                    }
                    Some(MatrixBackendRoomTimelineCommand::RetryLocalEcho { transaction_id, response }) => {
                        tracing::info!(
                            handle_id,
                            room_id,
                            transaction_id,
                            "Retrying (unwedging) local echo on active matrix-sdk room timeline"
                        );
                        let result = retry_local_echo_for_timeline(&timeline, &transaction_id).await;
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
                        let now = Instant::now();
                        let maybe_room = client.get_room(&parsed_room_id);
                        let mut new_state: HashMap<OwnedUserId, RoomTypingEntry> =
                            HashMap::with_capacity(typing_user_ids.len());
                        for user_id in typing_user_ids {
                            // Reuse the prior entry's display name when we
                            // already know it, so we don't hit the member
                            // store on every typing refresh.
                            let (display_name, first_seen_at) = match typing_state.remove(&user_id) {
                                Some(entry) => (entry.display_name, entry.first_seen_at),
                                None => {
                                    let name = if let Some(room) = maybe_room.as_ref() {
                                        match room.get_member_no_sync(&user_id).await {
                                            Ok(Some(member)) => member
                                                .display_name()
                                                .unwrap_or_else(|| user_id.as_str())
                                                .to_owned(),
                                            _ => user_id.to_string(),
                                        }
                                    } else {
                                        user_id.to_string()
                                    };
                                    (name, now)
                                }
                            };
                            new_state.insert(
                                user_id,
                                RoomTypingEntry {
                                    display_name,
                                    first_seen_at,
                                    last_seen_at: now,
                                },
                            );
                        }
                        typing_state = new_state;
                        publish_typing_state_if_changed(
                            handle_id,
                            &room_id,
                            &typing_state,
                            &mut last_published_typing,
                        );
                    }
                    Err(tokio::sync::broadcast::error::RecvError::Lagged(_)) => {
                        // Missed some events; the next event will reconcile,
                        // and the TTL prune below clears stale entries if
                        // no further event arrives.
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
                // TTL prune of typing entries lives here rather than in its
                // own `select!` arm: a sibling `tokio::time::sleep` arm gets
                // recreated each loop iteration and would lose the race
                // against this 50ms poll forever.
                if !typing_state.is_empty() && last_typing_prune.elapsed() >= TYPING_PRUNE_INTERVAL {
                    last_typing_prune = Instant::now();
                    let now = Instant::now();
                    typing_state
                        .retain(|_, entry| now.duration_since(entry.last_seen_at) < TYPING_USER_TTL);
                    publish_typing_state_if_changed(
                        handle_id,
                        &room_id,
                        &typing_state,
                        &mut last_published_typing,
                    );
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

/// Locate a local echo by transaction id (falls back to matrix-sdk-ui `unique_id`)
/// and call `SendHandle::abort()` on it to remove the wedged send-queue entry.
///
/// Returns `Ok(true)` when the echo was aborted, `Ok(false)` when the send
/// queue reports nothing to abort (already-sent or gone), and `Err(_)` when
/// the item cannot be located or has no send handle.
async fn cancel_local_echo_for_timeline(
    timeline: &matrix_sdk_ui::timeline::Timeline,
    transaction_id: &str,
) -> Result<bool, String> {
    let trimmed = transaction_id.trim();
    if trimmed.is_empty() {
        return Err("cannot cancel a local echo without a transaction id".to_owned());
    }

    let items = timeline.items().await;
    for item in items.iter() {
        let Some(event) = item.as_event() else {
            continue;
        };
        let matches_transaction = event
            .transaction_id()
            .map(|txn| txn.as_str() == trimmed)
            .unwrap_or(false);
        let matches_unique_id = item.unique_id().0 == trimmed;
        if !matches_transaction && !matches_unique_id {
            continue;
        }

        let Some(send_handle) = event.local_echo_send_handle() else {
            return Err(
                "timeline item is not a local echo (already sent or a remote event)".to_owned(),
            );
        };

        return send_handle
            .abort()
            .await
            .map_err(|e| format!("failed to cancel local echo: {e}"));
    }

    Err(format!(
        "no local echo found for transaction id '{trimmed}'"
    ))
}

/// Locate a local echo by transaction id (falls back to matrix-sdk-ui
/// `unique_id`) and call `SendHandle::unwedge()` on it to mark the wedged
/// send-queue entry for retry.
async fn retry_local_echo_for_timeline(
    timeline: &matrix_sdk_ui::timeline::Timeline,
    transaction_id: &str,
) -> Result<(), String> {
    let trimmed = transaction_id.trim();
    if trimmed.is_empty() {
        return Err("cannot retry a local echo without a transaction id".to_owned());
    }

    let items = timeline.items().await;
    for item in items.iter() {
        let Some(event) = item.as_event() else {
            continue;
        };
        let matches_transaction = event
            .transaction_id()
            .map(|txn| txn.as_str() == trimmed)
            .unwrap_or(false);
        let matches_unique_id = item.unique_id().0 == trimmed;
        if !matches_transaction && !matches_unique_id {
            continue;
        }

        let Some(send_handle) = event.local_echo_send_handle() else {
            return Err(
                "timeline item is not a local echo (already sent or a remote event)".to_owned(),
            );
        };

        return send_handle
            .unwedge()
            .await
            .map_err(|e| format!("failed to retry local echo: {e}"));
    }

    Err(format!(
        "no local echo found for transaction id '{trimmed}'"
    ))
}
