// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

pub async fn start_restored_backend(profile_id: &str) -> Result<MatrixBackendHandleInfo, String> {
    tracing::info!(profile_id, "Starting restored matrix-sdk backend runtime");

    let Some(restored) = bootstrap::restore_client(profile_id).await? else {
        tracing::info!(profile_id, "No persisted matrix-sdk session is available for restore");
        return Ok(MatrixBackendHandleInfo {
            handle_id: 0,
            has_session: false,
            auth_type: String::new(),
            homeserver_url: String::new(),
            user_id: String::new(),
            device_id: String::new(),
        });
    };

    let handle_id = NEXT_BACKEND_HANDLE_ID.fetch_add(1, Ordering::Relaxed);
    let verification_sessions = Arc::new(Mutex::new(HashMap::new()));
    let pending_verification_flow_ids = Arc::new(Mutex::new(Vec::new()));
    let verification_event_handlers = verification::install_incoming_verification_event_handlers(
        handle_id,
        restored.client.clone(),
        Arc::clone(&verification_sessions),
        Arc::clone(&pending_verification_flow_ids),
    );
    notifications::install_live_notification_handler(handle_id, restored.client.clone()).await;
    let call_event_handlers =
        runtime_calls::install_incoming_call_event_handlers(handle_id, restored.client.clone());
    let rtc_event_handlers =
        rtc::install_rtc_event_handlers(handle_id, restored.client.clone());
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .insert(
            handle_id,
            MatrixBackendHandle {
                client: restored.client,
                auth_failed: Arc::new(AtomicBool::new(false)),
                sync_task: None,
                media_proxy: None,
                room_list_snapshot: Arc::new(Mutex::new(Vec::new())),
                room_timeline_tasks: HashMap::new(),
                active_room_id: None,
                preferred_room_timeline_initial_page_size: ROOM_TIMELINE_INITIAL_PAGE_SIZE,
                room_timeline_snapshots: HashMap::new(),
                room_timeline_media_lookup: Arc::new(Mutex::new(HashMap::new())),
                preloaded_timelines: Arc::new(Mutex::new(HashMap::new())),
                thread_reply_counts: Arc::new(Mutex::new(HashMap::new())),
                thread_subscriptions: HashMap::new(),
                thread_subscription_lru: Vec::new(),
                active_thread_key: None,
                pending_identity_reset: Arc::new(Mutex::new(None)),
                pending_device_sign_out: Arc::new(Mutex::new(None)),
                verification_sessions,
                pending_verification_flow_ids,
                subscribed_rooms: subscriptions::SubscribedRooms::new(),
                _verification_event_handlers: verification_event_handlers,
                _call_event_handlers: call_event_handlers,
                _rtc_event_handlers: rtc_event_handlers,
            },
        );

    tracing::info!(
        profile_id,
        handle_id,
        user_id = %restored.user_id,
        device_id = %restored.device_id,
        homeserver_url = %restored.homeserver_url,
        "Started restored matrix-sdk backend runtime"
    );

    Ok(MatrixBackendHandleInfo {
        handle_id,
        has_session: true,
        auth_type: restored.auth_type,
        homeserver_url: restored.homeserver_url,
        user_id: restored.user_id,
        device_id: restored.device_id,
    })
}

pub async fn logout_backend(handle_id: u64) -> Result<(), String> {
    if handle_id == 0 {
        return Ok(());
    }

    let client = client_for_handle(handle_id)?;
    client
        .logout()
        .await
        .map_err(|e| format!("failed to log out matrix-sdk session: {e}"))
}

pub fn stop_backend(handle_id: u64) -> Result<(), String> {
    if handle_id == 0 {
        return Ok(());
    }

    let removed = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .remove(&handle_id);

    if let Some(mut handle) = removed {
        if let Some(proxy) = handle.media_proxy.take() {
            media_proxy::stop_proxy_instance(handle_id, proxy);
        }
        if let Some(sync_task) = handle.sync_task.take() {
            stop_sync_task(handle_id, sync_task);
        }
        for (_, task) in handle.room_timeline_tasks.drain() {
            stop_room_timeline_task(handle_id, task);
        }
        // Drop the handle (and the Client it owns) inside the tokio runtime
        // so that async destructors like the SQLite connection pool can run.
        crate::matrix_backend::ffi::runtime().block_on(async move {
            drop(handle);
        });
        tracing::info!(handle_id, "Stopped matrix-sdk backend runtime");
    } else {
        tracing::debug!(handle_id, "Matrix-sdk backend runtime handle was already absent");
    }
    Ok(())
}
