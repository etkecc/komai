// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Mutate or moderate existing timeline events: toggle reactions,
//! redact, and report.

use super::*;

pub async fn toggle_room_reaction(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reaction_key: &str,
) -> Result<(), String> {
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot toggle a matrix-sdk room reaction without an event id".to_owned());
    }

    let reaction_key = reaction_key.trim();
    if reaction_key.is_empty() {
        return Err("cannot toggle an empty matrix-sdk room reaction".to_owned());
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        reaction_key,
        "Toggling matrix-sdk room reaction"
    );

    // Dispatch to the active room timeline loop via its command channel.
    // The active timeline already has items loaded (including existing reactions),
    // so toggle_reaction can correctly detect whether to add or redact.
    // Building a fresh timeline via room.timeline() would lack this state
    // and fail to find existing reactions to remove.
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
        .send(MatrixBackendRoomTimelineCommand::ToggleReaction {
            event_id: event_id.to_owned(),
            reaction_key: reaction_key.to_owned(),
            response: response_tx,
        })
        .map_err(|_| "failed to send toggle-reaction command to active room timeline".to_owned())?;

    response_rx
        .await
        .map_err(|_| "active room timeline dropped the toggle-reaction response".to_owned())?
}

pub async fn redact_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
) -> Result<String, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot redact a matrix-sdk room event without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;
    let trimmed_reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        has_reason = trimmed_reason.is_some(),
        "Redacting matrix-sdk room event"
    );

    // Redaction is its own event, so it has its own ID; we were discarding it.
    room.redact(&parsed_event_id, trimmed_reason.as_deref(), None)
        .await
        .map(|response| response.event_id.to_string())
        .map_err(|e| format!("failed to redact matrix-sdk room event: {e}"))
}

pub async fn report_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot report a matrix-sdk room event without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;
    let trimmed_reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        has_reason = trimmed_reason.is_some(),
        "Reporting matrix-sdk room event"
    );

    room.report_content(parsed_event_id, trimmed_reason)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to report matrix-sdk room event: {e}"))
}
