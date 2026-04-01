// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::ruma::{events::AnySyncMessageLikeEvent, serde::Raw};
use serde_json::Value;

use super::*;

struct IncomingCallEvent {
    event_type: String,
    sender_id: String,
    event_id: String,
    content_json: String,
}

fn extract_incoming_call_event(raw: &Raw<AnySyncMessageLikeEvent>) -> Option<IncomingCallEvent> {
    let value = serde_json::from_str::<Value>(raw.json().get()).ok()?;
    let event_type = value.get("type")?.as_str()?.trim();
    if !matches!(
        event_type,
        "m.call.invite"
            | "m.call.candidates"
            | "m.call.answer"
            | "m.call.hangup"
            | "m.call.select_answer"
            | "m.call.reject"
            | "m.call.negotiate"
    ) {
        return None;
    }

    let sender_id = value.get("sender")?.as_str()?.trim();
    let event_id = value.get("event_id")?.as_str()?.trim();
    if sender_id.is_empty() || event_id.is_empty() {
        return None;
    }

    let content_json = serde_json::to_string(value.get("content")?).ok()?;
    Some(IncomingCallEvent {
        event_type: event_type.to_owned(),
        sender_id: sender_id.to_owned(),
        event_id: event_id.to_owned(),
        content_json,
    })
}

pub(crate) fn install_incoming_call_event_handlers(
    handle_id: u64,
    client: matrix_sdk::Client,
) -> Vec<EventHandlerDropGuard> {
    let room_handle = client.add_event_handler({
        move |raw: Raw<AnySyncMessageLikeEvent>, room: matrix_sdk::Room| async move {
            let Some(event) = extract_incoming_call_event(&raw) else {
                return;
            };

            let room_id = room.room_id().to_string();
            tracing::debug!(
                handle_id,
                room_id,
                event_type = %event.event_type,
                event_id = %event.event_id,
                sender_id = %event.sender_id,
                "Forwarding incoming matrix-sdk call event to C++ CallManager"
            );

            crate::ffi::matrix_notify_call_event_received(
                handle_id,
                &room_id,
                &event.event_type,
                &event.sender_id,
                &event.event_id,
                &event.content_json,
            );
        }
    });

    vec![client.event_handler_drop_guard(room_handle)]
}
