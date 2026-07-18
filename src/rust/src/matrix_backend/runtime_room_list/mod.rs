// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Re-export the runtime-scope items and external crate types the original
// flat file pulled in via direct `use` statements, at `pub(super)` so the
// child submodules can pick them up through `use super::*;`.

pub(super) use std::collections::{HashMap, HashSet};

pub(super) use super::*;
pub(super) use super::event_summary::summarize_sync_timeline_event;
pub(super) use matrix_sdk::{
    SessionChange,
    deserialized_responses::SyncOrStrippedState,
    event_handler::EventHandlerDropGuard,
    notification_settings::{NotificationSettings, RoomNotificationMode},
    ruma::{
        events::{
            AnySyncTimelineEvent, SyncStateEvent,
            ignored_user_list::IgnoredUserListEventContent,
            receipt::{ReceiptThread, ReceiptType, SyncReceiptEvent},
            room::join_rules::JoinRule,
            space::child::SpaceChildEventContent,
        },
        serde::Raw,
    },
};
pub(super) use matrix_sdk_base::latest_event::LatestEventValue as BaseLatestEventValue;
pub(super) use tokio::sync::broadcast::error::RecvError as BroadcastRecvError;

mod classify;
mod enrich;
mod snapshot;
mod sync_loop;

use sync_loop::run_sync_loop;

pub fn start_sync(handle_id: u64) -> Result<(), String> {
    let (client, room_list_snapshot) = {
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

        (handle.client.clone(), Arc::clone(&handle.room_list_snapshot))
    };

    let stop_requested = Arc::new(AtomicBool::new(false));
    let stop_requested_for_thread = Arc::clone(&stop_requested);
    let sync_task = std::thread::spawn(move || {
        crate::matrix_backend::ffi::runtime().block_on(run_sync_loop(
            handle_id,
            client,
            room_list_snapshot,
            stop_requested_for_thread,
        ));
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

pub async fn fetch_room_list(handle_id: u64) -> Result<Vec<MatrixRoomSummary>, String> {
    let snapshot = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| {
            handle
                .room_list_snapshot
                .lock()
                .expect("poisoned matrix room-list snapshot mutex")
                .clone()
        })
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;

    tracing::debug!(handle_id, room_count = snapshot.len(), "Fetched matrix room-list snapshot");

    Ok(snapshot)
}
