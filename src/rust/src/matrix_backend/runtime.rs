// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    collections::HashMap,
    sync::{
        Arc, Mutex, OnceLock,
        atomic::{AtomicBool, AtomicU64, Ordering},
    },
    thread::JoinHandle,
    time::Duration,
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

struct MatrixBackendHandle {
    client: Client,
    sync_task: Option<MatrixBackendSyncTask>,
}

struct MatrixBackendSyncTask {
    stop_requested: Arc<AtomicBool>,
    thread: JoinHandle<()>,
}

fn backend_handles() -> &'static Mutex<HashMap<u64, MatrixBackendHandle>> {
    static HANDLES: OnceLock<Mutex<HashMap<u64, MatrixBackendHandle>>> = OnceLock::new();
    HANDLES.get_or_init(|| Mutex::new(HashMap::new()))
}

fn client_for_handle(handle_id: u64) -> Result<Client, String> {
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| handle.client.clone())
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
        .insert(
            handle_id,
            MatrixBackendHandle {
                client: restored.client,
                sync_task: None,
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
            tracing::info!(handle_id, "Stopping matrix-sdk sync task");
            sync_task.stop_requested.store(true, Ordering::Relaxed);
            let _ = sync_task.thread.join();
        }
        tracing::info!(handle_id, "Stopped matrix-sdk backend runtime");
    } else {
        tracing::debug!(handle_id, "Matrix-sdk backend runtime handle was already absent");
    }
    Ok(())
}

pub fn start_sync(handle_id: u64) -> Result<(), String> {
    let client = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get_mut(&handle_id) else {
            return Err(format!("matrix-sdk backend runtime handle {handle_id} is not active"));
        };

        if let Some(sync_task) = handle.sync_task.as_ref() {
            if !sync_task.thread.is_finished() {
                tracing::debug!(handle_id, "Matrix-sdk sync task is already running");
                return Ok(());
            }
        }

        handle.client.clone()
    };

    let stop_requested = Arc::new(AtomicBool::new(false));
    let stop_requested_for_thread = Arc::clone(&stop_requested);
    let sync_task = std::thread::spawn(move || {
        crate::runtime().block_on(run_sync_loop(handle_id, client, stop_requested_for_thread));
    });

    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .entry(handle_id)
        .and_modify(|handle| {
            handle.sync_task = Some(MatrixBackendSyncTask {
                stop_requested,
                thread: sync_task,
            });
        });

    tracing::info!(handle_id, "Started matrix-sdk sync task");
    Ok(())
}

async fn run_sync_loop(handle_id: u64, client: Client, stop_requested: Arc<AtomicBool>) {
    let sync_settings = matrix_sdk::config::SyncSettings::new().timeout(Duration::from_secs(5));

    tracing::info!(handle_id, "Running matrix-sdk sync loop");

    while !stop_requested.load(Ordering::Relaxed) {
        match client.sync_once(sync_settings.clone()).await {
            Ok(response) => {
                tracing::debug!(
                    handle_id,
                    joined_rooms = response.rooms.joined.len(),
                    invited_rooms = response.rooms.invited.len(),
                    left_rooms = response.rooms.left.len(),
                    "Completed matrix-sdk sync iteration"
                );
            }
            Err(error) => {
                if stop_requested.load(Ordering::Relaxed) {
                    break;
                }

                tracing::warn!(handle_id, %error, "Matrix-sdk sync loop failed; retrying");
                tokio::time::sleep(Duration::from_secs(5)).await;
            }
        }
    }

    tracing::info!(handle_id, "Matrix-sdk sync loop stopped");
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
