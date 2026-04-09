// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Event inspection, frequent reactions, pinned events, permissions, and read receipts.

use super::*;

use matrix_sdk_base::event_cache::store::EventCacheStoreLockState;
use matrix_sdk::ruma::{
    EventId,
    events::{
        AnySyncMessageLikeEvent, AnySyncTimelineEvent,
        reaction::ReactionEventContent,
        receipt::{ReceiptThread, ReceiptType},
        room::pinned_events::RoomPinnedEventsEventContent,
    },
};

// ---------------------------------------------------------------------------
// Frequent reactions
// ---------------------------------------------------------------------------

pub async fn fetch_room_frequent_reactions(
    handle_id: u64,
    room_id: &str,
    lookback_days: i32,
    max_results: u32,
    max_scanned_events: u64,
) -> Result<Vec<String>, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let lookback_days = lookback_days.max(0);
    let max_results = usize::try_from(max_results).unwrap_or(usize::MAX);
    let max_scanned_events = usize::try_from(max_scanned_events).unwrap_or(usize::MAX);
    if max_results == 0 || max_scanned_events == 0 {
        return Ok(Vec::new());
    }

    let client = client_for_handle(handle_id)?;
    let store_guard = match client
        .event_cache_store()
        .lock()
        .await
        .map_err(|e| format!("failed to lock matrix-sdk event cache store: {e}"))?
    {
        EventCacheStoreLockState::Clean(guard) | EventCacheStoreLockState::Dirty(guard) => guard,
    };

    let mut reaction_events = store_guard
        .get_room_events(room.room_id(), Some("m.reaction"), None)
        .await
        .map_err(|e| format!("failed to load matrix-sdk room reactions from event cache: {e}"))?;

    reaction_events.sort_by(|lhs, rhs| rhs.timestamp().cmp(&lhs.timestamp()));

    let own_user_id = room.own_user_id().to_owned();
    let now_ms = u64::from(matrix_sdk::ruma::MilliSecondsSinceUnixEpoch::now().0);
    let cutoff_ms =
        now_ms.saturating_sub(u64::try_from(lookback_days).unwrap_or(0).saturating_mul(86_400_000));

    let mut frequency = HashMap::<String, usize>::new();
    let mut scanned = 0usize;

    for event in reaction_events {
        scanned += 1;
        if scanned > max_scanned_events {
            break;
        }

        let Some(timestamp) = event.timestamp() else {
            continue;
        };
        if u64::from(timestamp.0) < cutoff_ms {
            break;
        }

        let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::Reaction(reaction))) =
            event.raw().deserialize()
        else {
            continue;
        };
        let Some(reaction) = reaction.as_original() else {
            continue;
        };
        if reaction.sender != own_user_id {
            continue;
        }

        let normalized = normalize_reaction_for_comparison(&reaction.content);
        if normalized.is_empty() {
            continue;
        }

        *frequency.entry(normalized).or_default() += 1;
    }

    let mut sorted: Vec<_> = frequency.into_iter().collect();
    sorted.sort_by(|lhs, rhs| rhs.1.cmp(&lhs.1).then_with(|| lhs.0.cmp(&rhs.0)));

    Ok(sorted
        .into_iter()
        .take(max_results)
        .map(|(reaction, _count)| reaction)
        .collect())
}

fn normalize_reaction_for_comparison(content: &ReactionEventContent) -> String {
    content
        .relates_to
        .key
        .chars()
        .filter(|ch| *ch != '\u{FE0F}' && *ch != '\u{FE0E}')
        .collect()
}

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

pub async fn set_room_pinned_event_ids(
    handle_id: u64,
    room_id: &str,
    event_ids: Vec<String>,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    let pinned_event_ids = event_ids
        .iter()
        .map(|id| {
            EventId::parse(id.as_str())
                .map_err(|e| format!("invalid pinned event id '{id}': {e}"))
        })
        .collect::<Result<Vec<_>, _>>()?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        count = pinned_event_ids.len(),
        "Setting matrix-sdk room pinned events"
    );

    room.send_state_event(RoomPinnedEventsEventContent::new(pinned_event_ids))
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to set matrix-sdk room pinned events: {e}"))
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

    // Get the target event's timestamp so we can determine which members
    // have read *at least* up to this event (cumulative receipts).
    let target_ts = room
        .load_or_fetch_event(&parsed_event_id, None)
        .await
        .ok()
        .and_then(|e| e.timestamp())
        .map(|ts| u64::from(ts.0))
        .unwrap_or(0);

    let own_user_id = room.own_user_id().to_owned();
    let members = room
        .members(matrix_sdk::RoomMemberships::ACTIVE)
        .await
        .map_err(|e| format!("failed to load room members for read receipts: {e}"))?;

    let mut entries = Vec::new();
    for member in members.iter() {
        if member.user_id() == own_user_id {
            continue;
        }
        // Check Main (newer spec) then Unthreaded (legacy).
        for thread in [ReceiptThread::Main, ReceiptThread::Unthreaded] {
            if let Ok(Some((receipt_event_id, receipt))) =
                room.load_user_receipt(ReceiptType::Read, thread, member.user_id()).await
            {
                let receipt_ts = receipt.ts.map(|ts| u64::from(ts.0)).unwrap_or(0);
                // Include this member if their receipt targets this exact event,
                // or if the receipt timestamp is >= the event's origin_server_ts
                // (meaning they've read at least this far).
                if receipt_event_id == parsed_event_id || (target_ts > 0 && receipt_ts >= target_ts)
                {
                    entries.push(MatrixReadReceiptEntry {
                        user_id: member.user_id().to_string(),
                        display_name: member
                            .display_name()
                            .map(ToOwned::to_owned)
                            .unwrap_or_else(|| member.user_id().to_string()),
                        avatar_url: member
                            .avatar_url()
                            .map(ToString::to_string)
                            .map(normalize_mxc_uri)
                            .unwrap_or_default(),
                        timestamp: receipt_ts,
                    });
                    break;
                }
            }
        }
    }

    entries.sort_by(|a, b| b.timestamp.cmp(&a.timestamp).then_with(|| a.user_id.cmp(&b.user_id)));

    Ok(entries)
}
