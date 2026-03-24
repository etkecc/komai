// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    collections::HashMap,
    sync::{
        Mutex, OnceLock,
        atomic::{AtomicU64, Ordering},
    },
};

use matrix_sdk::Client;

use super::bootstrap;

pub struct MatrixBackendHandleInfo {
    pub handle_id: u64,
    pub has_session: bool,
    pub homeserver_url: String,
    pub user_id: String,
    pub device_id: String,
}

static NEXT_BACKEND_HANDLE_ID: AtomicU64 = AtomicU64::new(1);

fn backend_handles() -> &'static Mutex<HashMap<u64, Client>> {
    static HANDLES: OnceLock<Mutex<HashMap<u64, Client>>> = OnceLock::new();
    HANDLES.get_or_init(|| Mutex::new(HashMap::new()))
}

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
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .insert(handle_id, restored.client);

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

    if removed.is_some() {
        tracing::info!(handle_id, "Stopped matrix-sdk backend runtime");
    } else {
        tracing::debug!(handle_id, "Matrix-sdk backend runtime handle was already absent");
    }
    Ok(())
}
