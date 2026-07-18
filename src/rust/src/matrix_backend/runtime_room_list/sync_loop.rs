// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Sliding-sync driver: spawns the SyncService, registers latest-event
//! listeners on newly-visible rooms, and republishes the room-list
//! snapshot on every diff.

use super::*;
use super::snapshot::{build_room_list_snapshot, load_ignored_user_ids};


pub(super) async fn run_sync_loop(
    handle_id: u64,
    client: Client,
    room_list_snapshot: Arc<Mutex<Vec<MatrixRoomSummary>>>,
    stop_requested: Arc<AtomicBool>,
) {
    tracing::info!(handle_id, "Running matrix-sdk room-list sync loop");

    let sync_service = match SyncService::builder(client.clone())
        .with_offline_mode()
        .build()
        .await
    {
        Ok(service) => service,
        Err(error) => {
            tracing::warn!(handle_id, %error, "Failed to create matrix-sdk-ui SyncService");
            return;
        }
    };

    let room_list_service = sync_service.room_list_service();

    let room_list = match room_list_service.all_rooms().await {
        Ok(room_list) => room_list,
        Err(error) => {
            tracing::warn!(handle_id, %error, "Failed to acquire matrix-sdk-ui room list");
            return;
        }
    };

    let (entries_stream, entries_controller) =
        room_list.entries_with_dynamic_adapters(ROOM_LIST_PAGE_SIZE);
    if !entries_controller.set_filter(Box::new(filters::new_filter_non_left())) {
        tracing::warn!(handle_id, "Failed to install matrix-sdk-ui room-list filter");
    }

    sync_service.start().await;

    // Spawn the room-subscription reconciler. It applies FFI-driven
    // subscribe/unsubscribe changes to sliding-sync room subscriptions,
    // and forwards per-room `pinned_event_ids_stream` updates to the UI.
    // See `runtime_subscriptions.rs` for the full design.
    let reconciler_stop = Arc::clone(&stop_requested);
    let reconciler_handle = match super::subscribed_rooms_for_handle(handle_id) {
        Ok(subscribed_rooms) => {
            let reconciler_client = client.clone();
            let reconciler_room_list_service = Arc::clone(&room_list_service);
            Some(tokio::spawn(super::subscriptions::run_reconciler(
                handle_id,
                reconciler_client,
                reconciler_room_list_service,
                subscribed_rooms,
                reconciler_stop,
            )))
        }
        Err(error) => {
            tracing::warn!(
                handle_id,
                %error,
                "Failed to acquire subscribed-rooms state; live pinned-events updates will not work"
            );
            None
        }
    };

    let mut state_subscriber = sync_service.state();
    let mut entries_stream = Box::pin(entries_stream);
    let mut ignored_user_list_changes = Some(Box::pin(client.subscribe_to_ignore_user_list_changes()));

    // The SDK broadcasts `SessionChange::UnknownToken` when a request got a
    // 401 and the token could not be refreshed (e.g. the OAuth refresh token
    // was rejected with invalid_grant). This is the authoritative "the session
    // is dead, sign the user out" signal: with `with_offline_mode()` enabled
    // above, an auth failure never surfaces as `SyncServiceState::Error` —
    // the sync service treats it like lost connectivity and ping-pongs
    // between Offline and Running forever, re-hitting the homeserver and the
    // token endpoint on every cycle. Reacting here stops that loop and drops
    // the user to the login page instead.
    let mut session_changes = Some(client.subscribe_to_session_changes());

    // Held for the lifetime of the loop so its `PushRulesEvent` handler stays
    // registered and we don't churn handlers on each per-room mode lookup.
    let notification_settings = client.notification_settings().await;
    let mut notification_settings_changes = notification_settings.subscribe_to_changes();

    // Wake the loop when our own user's read receipts arrive, so the
    // thread-receipt unread-suppression heuristic in `room_list_item_to_summary`
    // re-evaluates without waiting for an unrelated `entries_stream` diff.
    // Cross-device thread reads (e.g. Element X reading inside a thread) don't
    // change matrix-sdk-base's `num_unread_messages`, so they never reach us
    // via `RoomInfoNotableUpdateReasons::READ_RECEIPT` or via the room-list
    // entries stream — we have to listen for the raw ephemeral ourselves.
    //
    // The wake-up triggers a *full* snapshot rebuild (every room), matching
    // the `notification_settings_changes` arm below. With the thread-scoped
    // filter on the handler this is bounded — receipts only ping the loop
    // when they're for our user *and* tied to a specific thread, which is
    // rare enough not to matter today. If profiling later shows this rebuild
    // dominating, the next step is to scope it to just the affected room:
    // the handler already receives the `Room` arg, so we could splice a
    // single updated `MatrixRoomSummary` into `room_list_snapshot` instead
    // of rebuilding all rooms. That requires a bit of refactoring because
    // the parent-space enrichment pass at the tail of `build_room_list_snapshot`
    // currently iterates every room; a per-room path would either skip that
    // enrichment (parent spaces don't change on a receipt) or fold it in
    // separately.
    let receipt_notify = Arc::new(tokio::sync::Notify::new());
    let _own_receipt_handler_guard: Option<EventHandlerDropGuard> = match client.user_id() {
        Some(user_id) => {
            let our_user_id = user_id.to_owned();
            let notify = Arc::clone(&receipt_notify);
            let handle = client.add_event_handler({
                move |event: SyncReceiptEvent, _: matrix_sdk::Room| {
                    let notify = Arc::clone(&notify);
                    let our_user_id = our_user_id.clone();
                    async move {
                        // Only fire for *thread-scoped* receipts of our own user.
                        // Main/Unthreaded receipts already update matrix-sdk-base's
                        // `RoomInfo::read_receipts`, which fires `RoomInfoNotableUpdate`
                        // and reaches us via the room-list `entries_stream`. Listening
                        // for those here too would double-rebuild the snapshot on every
                        // locally-read message.
                        let needs_rebuild = event.content.0.values().any(|by_type| {
                            by_type.values().any(|by_user| {
                                by_user.get(&our_user_id).is_some_and(|receipt| {
                                    matches!(receipt.thread, ReceiptThread::Thread(_))
                                })
                            })
                        });
                        if needs_rebuild {
                            notify.notify_one();
                        }
                    }
                }
            });
            Some(client.event_handler_drop_guard(handle))
        }
        None => {
            tracing::warn!(
                handle_id,
                "Client has no user id; cross-device receipt watcher disabled"
            );
            None
        }
    };

    let mut current_values = Vector::<RoomListItem>::new();
    let mut initial_sync_ready_notified = false;
    let mut sync_connected = true;
    // Rooms we have already registered with the SDK's `LatestEvents`
    // subsystem. See `listen_to_latest_events_for_new_rooms` below for
    // why we need to do this ourselves.
    let mut rooms_listening_latest_events: HashSet<OwnedRoomId> = HashSet::new();
    crate::ffi::matrix_notify_ignored_user_list_updated(
        handle_id,
        load_ignored_user_ids(&client).await,
    );

    while !stop_requested.load(Ordering::Relaxed) {
        tokio::select! {
            maybe_diffs = entries_stream.next() => {
                match maybe_diffs {
                    Some(diffs) => {
                        let diffs: Vec<VectorDiff<RoomListItem>> = diffs;
                        for diff in diffs.iter().cloned() {
                            diff.apply(&mut current_values);
                        }

                        listen_to_latest_events_for_new_rooms(
                            &client,
                            &current_values,
                            &mut rooms_listening_latest_events,
                        )
                        .await;

                        let snapshot = build_room_list_snapshot(
                            &current_values,
                            &notification_settings,
                        )
                        .await;
                        let room_count = snapshot.len();
                        let ffi_snapshot = snapshot
                            .iter()
                            .cloned()
                            .map(crate::matrix_backend::ffi::into_ffi_matrix_room_summary)
                            .collect();
                        *room_list_snapshot
                            .lock()
                            .expect("poisoned matrix room-list snapshot mutex") = snapshot;
                        crate::ffi::matrix_notify_room_list_snapshot_updated(
                            handle_id,
                            ffi_snapshot,
                        );

                        if !initial_sync_ready_notified {
                            crate::ffi::matrix_notify_initial_sync_ready(handle_id);
                            initial_sync_ready_notified = true;

                            if let Err(error) = super::preloader::start_preload(handle_id) {
                                tracing::warn!(handle_id, %error, "Failed to start background preloader");
                            }

                            tracing::info!(
                                handle_id,
                                room_count,
                                "Completed initial matrix-sdk-ui room-list sync iteration"
                            );
                        } else {
                            tracing::debug!(
                                handle_id,
                                room_count,
                                "Updated matrix-sdk room-list snapshot"
                            );
                        }
                    }
                    None => {
                        tracing::info!(handle_id, "Matrix-sdk-ui room-list entries stream ended");
                        break;
                    }
                }
            }

            maybe_state = state_subscriber.next() => {
                match maybe_state {
                    Some(SyncServiceState::Running) => {
                        if !sync_connected {
                            sync_connected = true;
                            crate::ffi::matrix_notify_sync_connection_state_changed(handle_id, true);
                            tracing::info!(handle_id, "Matrix-sdk-ui sync service recovered");
                        }
                    }
                    Some(SyncServiceState::Error(error)) => {
                        let error_str = format!("{error}");
                        let is_auth_error = is_auth_failure(&error_str);
                        tracing::warn!(
                            handle_id,
                            %error,
                            is_auth_error,
                            "Matrix-sdk-ui sync service error"
                        );

                        if is_auth_error {
                            crate::ffi::matrix_notify_sync_stopped(
                                handle_id,
                                &error_str,
                                true,
                            );
                            break;
                        }

                        if sync_connected {
                            sync_connected = false;
                            crate::ffi::matrix_notify_sync_connection_state_changed(
                                handle_id,
                                false,
                            );
                        }

                        // Attempt to restart after non-auth errors.
                        sync_service.start().await;
                    }
                    Some(SyncServiceState::Offline) => {
                        tracing::info!(handle_id, "Matrix-sdk-ui sync service entered offline mode");
                        if sync_connected {
                            sync_connected = false;
                            crate::ffi::matrix_notify_sync_connection_state_changed(
                                handle_id,
                                false,
                            );
                        }
                    }
                    Some(SyncServiceState::Terminated) => {
                        tracing::info!(handle_id, "Matrix-sdk-ui sync service terminated");
                        break;
                    }
                    Some(SyncServiceState::Idle) => {
                        tracing::debug!(handle_id, "Matrix-sdk-ui sync service idle");
                    }
                    None => {
                        tracing::info!(handle_id, "Matrix-sdk-ui sync service state stream ended");
                        break;
                    }
                }
            }

            maybe_session_change = async {
                match session_changes.as_mut() {
                    Some(changes) => changes.recv().await,
                    None => std::future::pending().await,
                }
            } => {
                match maybe_session_change {
                    Ok(SessionChange::UnknownToken(unknown_token_data)) => {
                        tracing::warn!(
                            handle_id,
                            soft_logout = unknown_token_data.soft_logout,
                            "Matrix-sdk session token is no longer valid and could not be \
                             refreshed; stopping sync"
                        );
                        super::mark_handle_auth_failed(handle_id);
                        crate::ffi::matrix_notify_sync_stopped(
                            handle_id,
                            "the access token is no longer valid and could not be refreshed",
                            true,
                        );
                        break;
                    }
                    Ok(SessionChange::TokensRefreshed) => {
                        tracing::debug!(handle_id, "Matrix-sdk session tokens were refreshed");
                    }
                    Err(BroadcastRecvError::Lagged(_)) => {}
                    Err(BroadcastRecvError::Closed) => {
                        tracing::info!(handle_id, "Matrix-sdk session change stream ended");
                        session_changes = None;
                    }
                }
            }

            maybe_ignored = async {
                match ignored_user_list_changes.as_mut() {
                    Some(changes) => changes.next().await,
                    None => std::future::pending().await,
                }
            } => {
                match maybe_ignored {
                    Some(user_ids) => {
                        let mut user_ids: Vec<String> = user_ids;
                        user_ids.sort();
                        user_ids.dedup();
                        crate::ffi::matrix_notify_ignored_user_list_updated(handle_id, user_ids.clone());
                        tracing::debug!(
                            handle_id,
                            ignored_user_count = user_ids.len(),
                            "Updated matrix-sdk ignored-user list snapshot"
                        );
                    }
                    None => {
                        ignored_user_list_changes = None;
                        tracing::info!(handle_id, "Matrix-sdk ignored-user list stream ended");
                    }
                }
            }

            notification_change = notification_settings_changes.recv() => {
                // Push rules changed locally or via another session — rebuild
                // so unread badges reflect new modes without waiting for the
                // next event in each room.
                match notification_change {
                    Ok(()) | Err(BroadcastRecvError::Lagged(_)) => {
                        if !current_values.is_empty() {
                            let snapshot = build_room_list_snapshot(
                                &current_values,
                                &notification_settings,
                            )
                            .await;
                            let room_count = snapshot.len();
                            let ffi_snapshot = snapshot
                                .iter()
                                .cloned()
                                .map(crate::matrix_backend::ffi::into_ffi_matrix_room_summary)
                                .collect();
                            *room_list_snapshot
                                .lock()
                                .expect("poisoned matrix room-list snapshot mutex") = snapshot;
                            crate::ffi::matrix_notify_room_list_snapshot_updated(
                                handle_id,
                                ffi_snapshot,
                            );
                            tracing::debug!(
                                handle_id,
                                room_count,
                                "Rebuilt matrix-sdk room-list snapshot after notification settings change"
                            );
                        }
                    }
                    Err(BroadcastRecvError::Closed) => {
                        // Should not happen while we hold `notification_settings`;
                        // resubscribe to avoid a tight loop on a closed receiver.
                        tracing::warn!(
                            handle_id,
                            "Notification-settings change stream closed unexpectedly"
                        );
                        notification_settings_changes = notification_settings.subscribe_to_changes();
                    }
                }
            }

            _ = receipt_notify.notified() => {
                // Our own user's read receipt arrived (typically from another
                // device). Rebuild so the thread-receipt suppression in
                // `room_list_item_to_summary` re-evaluates against the freshly
                // stored receipt.
                if !current_values.is_empty() {
                    let snapshot = build_room_list_snapshot(
                        &current_values,
                        &notification_settings,
                    )
                    .await;
                    let room_count = snapshot.len();
                    let ffi_snapshot = snapshot
                        .iter()
                        .cloned()
                        .map(crate::matrix_backend::ffi::into_ffi_matrix_room_summary)
                        .collect();
                    *room_list_snapshot
                        .lock()
                        .expect("poisoned matrix room-list snapshot mutex") = snapshot;
                    crate::ffi::matrix_notify_room_list_snapshot_updated(
                        handle_id,
                        ffi_snapshot,
                    );
                    tracing::debug!(
                        handle_id,
                        room_count,
                        "Rebuilt matrix-sdk room-list snapshot after own-user read receipt"
                    );
                }
            }

            _ = tokio::time::sleep(Duration::from_millis(200)) => {
                if stop_requested.load(Ordering::Relaxed) {
                    break;
                }
            }
        }
    }

    sync_service.stop().await;

    if let Some(handle) = reconciler_handle {
        // The reconciler observes `stop_requested` within ~250ms and exits
        // cleanly. Await it so any in-flight `subscribe_to_rooms` call
        // finishes before we return.
        let _ = handle.await;
    }

    tracing::info!(handle_id, "Matrix-sdk room-list sync loop stopped");
}

/// Register every non-invite, non-space room we know about with the SDK's
/// `LatestEvents` subsystem so it computes and persists `RoomInfo::latest_event`
/// for each.
///
/// The SDK only auto-registers rooms that enter a sliding-sync response window;
/// rooms living outside the window never get a latest event computed, so
/// `matrix_sdk_base::Room::latest_event()` keeps returning
/// `LatestEventValue::None`, which in turn blanks the room-list preview on
/// every snapshot rebuild. Registering every room we've ever seen — even ones
/// not currently visible — keeps the preview stable.
///
/// We do this unconditionally (rather than conditionally on a user setting),
/// so that toggling a preview visibility setting at runtime does not require
/// re-populating caches.
pub(super) async fn listen_to_latest_events_for_new_rooms(
    client: &Client,
    rooms: &Vector<RoomListItem>,
    already_listening: &mut HashSet<OwnedRoomId>,
) {
    let mut to_register: Vec<OwnedRoomId> = Vec::new();
    for room in rooms.iter() {
        if room.is_space() {
            continue;
        }
        if matches!(room.state(), RoomState::Invited) {
            continue;
        }
        let room_id = room.room_id().to_owned();
        if already_listening.contains(&room_id) {
            continue;
        }
        to_register.push(room_id);
    }

    if to_register.is_empty() {
        return;
    }

    let latest_events = client.latest_events().await;
    for room_id in to_register {
        match latest_events.listen_to_room(&room_id).await {
            Ok(_) => {}
            Err(error) => {
                tracing::debug!(
                    %room_id,
                    %error,
                    "Failed to register room with matrix-sdk latest-events subsystem"
                );
            }
        }
        already_listening.insert(room_id);
    }
}

pub(super) fn is_auth_failure(error_message: &str) -> bool {
    let lower = error_message.to_lowercase();
    lower.contains("invalid_grant")
        || lower.contains("invalid grant")
        || lower.contains("refresh_token")
        || lower.contains("m_unknown_token")
        || lower.contains("unknown token")
}
