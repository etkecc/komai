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
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .insert(
            handle_id,
            MatrixBackendHandle {
                client: restored.client,
                sync_task: None,
                room_list_snapshot: Arc::new(Mutex::new(Vec::new())),
                room_timeline_task: None,
                room_timeline_snapshot: Arc::new(Mutex::new(Vec::new())),
                room_timeline_media_lookup: Arc::new(Mutex::new(HashMap::new())),
                pending_identity_reset: Arc::new(Mutex::new(None)),
                verification_sessions,
                pending_verification_flow_ids,
                _verification_event_handlers: verification_event_handlers,
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
        homeserver_url: restored.homeserver_url,
        user_id: restored.user_id,
        device_id: restored.device_id,
    })
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
        if let Some(sync_task) = handle.sync_task.take() {
            stop_sync_task(handle_id, sync_task);
        }
        if let Some(room_timeline_task) = handle.room_timeline_task.take() {
            stop_room_timeline_task(handle_id, room_timeline_task);
        }
        tracing::info!(handle_id, "Stopped matrix-sdk backend runtime");
    } else {
        tracing::debug!(handle_id, "Matrix-sdk backend runtime handle was already absent");
    }
    Ok(())
}
