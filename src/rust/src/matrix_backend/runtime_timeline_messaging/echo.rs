// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Manipulate the SDK's pending local-echo queue: cancel an in-flight
//! send, or retry one that failed.

use super::*;

pub async fn cancel_local_echo(
    handle_id: u64,
    room_id: &str,
    transaction_id: &str,
) -> Result<bool, String> {
    let transaction_id = transaction_id.trim();
    if transaction_id.is_empty() {
        return Err("cannot cancel a local echo without a transaction id".to_owned());
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        transaction_id,
        "Cancelling matrix-sdk local echo"
    );

    let command_sender = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles
            .get(&handle_id)
            .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;
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
        task.commands.clone()
    };

    let (response_tx, response_rx) = tokio::sync::oneshot::channel();
    command_sender
        .send(MatrixBackendRoomTimelineCommand::CancelLocalEcho {
            transaction_id: transaction_id.to_owned(),
            response: response_tx,
        })
        .map_err(|_| {
            "failed to send cancel-local-echo command to active room timeline".to_owned()
        })?;

    response_rx
        .await
        .map_err(|_| "active room timeline dropped the cancel-local-echo response".to_owned())?
}

pub async fn retry_local_echo(
    handle_id: u64,
    room_id: &str,
    transaction_id: &str,
) -> Result<(), String> {
    let transaction_id = transaction_id.trim();
    if transaction_id.is_empty() {
        return Err("cannot retry a local echo without a transaction id".to_owned());
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        transaction_id,
        "Retrying matrix-sdk local echo"
    );

    let command_sender = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles
            .get(&handle_id)
            .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;
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
        task.commands.clone()
    };

    let (response_tx, response_rx) = tokio::sync::oneshot::channel();
    command_sender
        .send(MatrixBackendRoomTimelineCommand::RetryLocalEcho {
            transaction_id: transaction_id.to_owned(),
            response: response_tx,
        })
        .map_err(|_| {
            "failed to send retry-local-echo command to active room timeline".to_owned()
        })?;

    response_rx
        .await
        .map_err(|_| "active room timeline dropped the retry-local-echo response".to_owned())?
}
