// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Event inspection, pinned events, permissions, and read receipts.

use super::*;

use matrix_sdk::ruma::{
    EventId,
    events::receipt::{ReceiptThread, ReceiptType},
    events::room::pinned_events::RoomPinnedEventsEventContent,
};

// ---------------------------------------------------------------------------
// Pinned events
// ---------------------------------------------------------------------------

pub async fn fetch_room_pinned_event_ids(
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<String>, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    load_cached_pinned_event_ids(&room).await
}

pub async fn pin_room_event(handle_id: u64, room_id: &str, event_id: &str) -> Result<(), String> {
    update_room_pinned_event_ids(handle_id, room_id, event_id, true).await
}

pub async fn unpin_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(), String> {
    update_room_pinned_event_ids(handle_id, room_id, event_id, false).await
}

async fn update_room_pinned_event_ids(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    should_pin: bool,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot update matrix-sdk room pinned events without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;
    let mut pinned_event_ids = load_cached_pinned_event_ids(&room)
        .await?
        .into_iter()
        .map(|event_id| {
            EventId::parse(&event_id)
                .map_err(|e| format!("invalid pinned event id '{event_id}' from state store: {e}"))
        })
        .collect::<Result<Vec<_>, _>>()?;

    let mut changed = false;
    if should_pin {
        if !pinned_event_ids.iter().any(|candidate| candidate == &parsed_event_id) {
            pinned_event_ids.push(parsed_event_id.clone());
            changed = true;
        }
    } else {
        let original_len = pinned_event_ids.len();
        pinned_event_ids.retain(|candidate| candidate != &parsed_event_id);
        changed = pinned_event_ids.len() != original_len;
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        should_pin,
        changed,
        "Updating matrix-sdk room pinned events"
    );

    if !changed {
        return Ok(());
    }

    room.send_state_event(RoomPinnedEventsEventContent::new(pinned_event_ids))
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to update matrix-sdk room pinned events: {e}"))
}

async fn load_cached_pinned_event_ids(room: &Room) -> Result<Vec<String>, String> {
    let Some(raw_event) = room
        .get_state_event_static::<RoomPinnedEventsEventContent>()
        .await
        .map_err(|e| format!("failed to load matrix-sdk room pinned events from state store: {e}"))?
    else {
        return Ok(Vec::new());
    };

    let event = raw_event.deserialize().map_err(|e| {
        format!("failed to deserialize matrix-sdk room pinned events from state store: {e}")
    })?;

    Ok(match event {
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Sync(ev) => ev
            .as_original()
            .map(|ev| {
                ev.content
                    .pinned
                    .iter()
                    .map(|event_id| event_id.to_string())
                    .collect()
            })
            .unwrap_or_default(),
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Stripped(_) => Vec::new(),
    })
}

// ---------------------------------------------------------------------------
// Permissions
// ---------------------------------------------------------------------------

pub async fn fetch_room_redaction_permissions(
    handle_id: u64,
    room_id: &str,
) -> Result<MatrixRoomRedactionPermissions, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let own_user_id = room.own_user_id().to_owned();
    let member = room
        .get_member(&own_user_id)
        .await
        .map_err(|e| format!("failed to fetch matrix-sdk room member permissions: {e}"))?
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} cannot resolve own member state in room {}",
                room_id.trim()
            )
        })?;

    Ok(MatrixRoomRedactionPermissions {
        can_redact_own: member.can_redact_own(),
        can_redact_other: member.can_redact_other(),
    })
}

// ---------------------------------------------------------------------------
// Event inspection
// ---------------------------------------------------------------------------

pub struct RawEventDialogData {
    pub pretty_json: String,
    pub body: String,
    pub formatted_body: String,
}

/// Extracts the `type` and `content` JSON from a timeline event for forwarding.
///
/// Returns `(event_type, content_json)` — e.g. `("m.room.message", "{\"body\":...}")`.
/// The content JSON is the raw, unmodified event content from the server, preserving
/// all metadata fields (width, height, duration, thumbnail, blurhash, etc.).
pub async fn fetch_active_room_event_content_for_forwarding(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(String, String), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot extract event content without an event id".to_owned());
    }

    let timeline = room
        .timeline()
        .await
        .map_err(|e| format!("failed to build matrix-sdk room timeline for forwarding: {e}"))?;
    let items = timeline.items().await;

    for item in items.iter() {
        let Some(event) = item.as_event() else {
            continue;
        };
        let Some(current_event_id) = event.event_id() else {
            continue;
        };
        if current_event_id.as_str() != event_id {
            continue;
        }

        let raw_event = event.latest_json().ok_or_else(|| {
            format!("matrix-sdk room event '{event_id}' has no raw JSON available for forwarding")
        })?;

        let parsed: serde_json::Value =
            serde_json::from_str(raw_event.json().get()).map_err(|e| {
                format!("failed to parse raw JSON for matrix-sdk room event '{event_id}': {e}")
            })?;

        let event_type = parsed
            .get("type")
            .and_then(|v| v.as_str())
            .unwrap_or("m.room.message")
            .to_owned();

        let content = parsed
            .get("content")
            .ok_or_else(|| {
                format!("matrix-sdk room event '{event_id}' has no content field")
            })?;

        let content_json = serde_json::to_string(content).map_err(|e| {
            format!("failed to serialize content of matrix-sdk room event '{event_id}': {e}")
        })?;

        return Ok((event_type, content_json));
    }

    Err(format!(
        "matrix-sdk room timeline for '{}' does not currently include event '{event_id}'",
        room_id.trim(),
    ))
}

pub async fn fetch_active_room_raw_event_dialog_data(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<RawEventDialogData, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err(
            "cannot inspect a matrix-sdk room event without an event id".to_owned(),
        );
    }

    let timeline = room
        .timeline()
        .await
        .map_err(|e| format!("failed to build matrix-sdk room timeline for raw inspection: {e}"))?;
    let items = timeline.items().await;

    for item in items.iter() {
        let Some(event) = item.as_event() else {
            continue;
        };
        let Some(current_event_id) = event.event_id() else {
            continue;
        };
        if current_event_id.as_str() != event_id {
            continue;
        }

        let raw_event = event.latest_json().ok_or_else(|| {
            format!(
                "matrix-sdk room event '{event_id}' does not currently have raw JSON available"
            )
        })?;

        let raw_json_str = raw_event.json().get();
        let parsed: serde_json::Value = serde_json::from_str(raw_json_str).map_err(|e| {
            format!("failed to parse raw JSON for matrix-sdk room event '{event_id}': {e}")
        })?;

        let pretty_json = {
            let mut buf = Vec::new();
            let formatter = serde_json::ser::PrettyFormatter::with_indent(b"    ");
            let mut serializer = serde_json::Serializer::with_formatter(&mut buf, formatter);
            serde::Serialize::serialize(&parsed, &mut serializer)
                .ok()
                .and_then(|_| String::from_utf8(buf).ok())
                .unwrap_or_else(|| raw_json_str.to_owned())
        };

        let body = parsed
            .get("content")
            .and_then(|c| c.get("body"))
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_owned();

        let formatted_body = parsed
            .get("content")
            .and_then(|c| c.get("formatted_body"))
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_owned();

        return Ok(RawEventDialogData {
            pretty_json,
            body,
            formatted_body,
        });
    }

    Err(format!(
        "matrix-sdk room timeline for '{}' does not currently include event '{}'",
        room_id.trim(),
        event_id
    ))
}

// ---------------------------------------------------------------------------
// Read receipts
// ---------------------------------------------------------------------------

pub async fn fetch_room_read_receipts(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<Vec<MatrixReadReceiptEntry>, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err(
            "cannot inspect matrix-sdk room read receipts without an event id".to_owned(),
        );
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;

    let mut receipts = room
        .load_event_receipts(ReceiptType::Read, ReceiptThread::Unthreaded, &parsed_event_id)
        .await
        .map_err(|e| format!("failed to load matrix-sdk room read receipts: {e}"))?;

    receipts.sort_by(|a, b| {
        let a_ts = a.1.ts.map(|ts| u64::from(ts.0)).unwrap_or(0);
        let b_ts = b.1.ts.map(|ts| u64::from(ts.0)).unwrap_or(0);
        b_ts.cmp(&a_ts).then_with(|| a.0.as_str().cmp(b.0.as_str()))
    });

    let mut entries = Vec::with_capacity(receipts.len());
    for (user_id, receipt) in receipts {
        let member = room
            .get_member(&user_id)
            .await
            .map_err(|e| format!("failed to fetch matrix-sdk room member for receipt: {e}"))?;
        let display_name = member
            .as_ref()
            .and_then(|member| member.display_name().map(ToOwned::to_owned))
            .unwrap_or_else(|| user_id.to_string());
        let avatar_url = member
            .as_ref()
            .and_then(|member| member.avatar_url().map(ToString::to_string))
            .map(normalize_mxc_uri)
            .unwrap_or_default();
        let timestamp = receipt.ts.map(|ts| u64::from(ts.0)).unwrap_or(0);

        entries.push(MatrixReadReceiptEntry {
            user_id: user_id.to_string(),
            display_name,
            avatar_url,
            timestamp,
        });
    }

    Ok(entries)
}
