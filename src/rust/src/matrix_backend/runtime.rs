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

use matrix_sdk::{
    Client,
    ruma::api::client::profile::{AvatarUrl, DisplayName},
};

use super::bootstrap;

pub struct MatrixBackendHandleInfo {
    pub handle_id: u64,
    pub has_session: bool,
    pub homeserver_url: String,
    pub user_id: String,
    pub device_id: String,
}

pub struct MatrixOwnProfile {
    pub display_name: String,
    pub avatar_url: String,
}

static NEXT_BACKEND_HANDLE_ID: AtomicU64 = AtomicU64::new(1);

fn backend_handles() -> &'static Mutex<HashMap<u64, Client>> {
    static HANDLES: OnceLock<Mutex<HashMap<u64, Client>>> = OnceLock::new();
    HANDLES.get_or_init(|| Mutex::new(HashMap::new()))
}

fn client_for_handle(handle_id: u64) -> Result<Client, String> {
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .cloned()
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))
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

pub async fn fetch_own_profile(handle_id: u64) -> Result<MatrixOwnProfile, String> {
    let client = client_for_handle(handle_id)?;
    let user_id = client
        .user_id()
        .map(|user_id| user_id.to_string())
        .unwrap_or_default();

    tracing::debug!(handle_id, user_id, "Fetching own profile via matrix-sdk backend runtime");

    let profile = client
        .account()
        .fetch_user_profile()
        .await
        .map_err(|e| format!("failed to fetch own profile via matrix-sdk: {e}"))?;

    let display_name = profile
        .get_static::<DisplayName>()
        .map_err(|e| format!("failed to parse display name from matrix-sdk profile response: {e}"))?
        .unwrap_or_default();

    let avatar_url = profile
        .get_static::<AvatarUrl>()
        .map_err(|e| format!("failed to parse avatar URL from matrix-sdk profile response: {e}"))?
        .map(|url| url.to_string())
        .unwrap_or_default();

    tracing::debug!(
        handle_id,
        user_id,
        has_display_name = !display_name.is_empty(),
        has_avatar_url = !avatar_url.is_empty(),
        "Fetched own profile via matrix-sdk backend runtime"
    );

    Ok(MatrixOwnProfile {
        display_name,
        avatar_url,
    })
}
