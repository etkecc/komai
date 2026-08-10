// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Chat-history export: walk a room's full history backwards over
//! `/rooms/{id}/messages` in bounded batches. The C++ side drives the loop
//! (one FFI call per batch), buffers the items, and renders the transcript;
//! this module stays stateless so cancellation and progress need no
//! Rust-side machinery.

use super::*;

use matrix_sdk::deserialized_responses::TimelineEvent;
use matrix_sdk::room::MessagesOptions;
use matrix_sdk::ruma::UInt;
use matrix_sdk::ruma::events::AnySyncTimelineEvent;

pub struct ChatExportEvent {
    pub item: MatrixTimelineItem,
    /// `""` | `"annotation"` | `"replacement"` — from `content.m.relates_to`.
    pub relation_kind: String,
    /// Target of the relation. Empty when `relation_kind` is empty.
    pub relates_to_event_id: String,
    /// Reaction key (emoji) when `relation_kind` is `"annotation"`.
    pub annotation_key: String,
}

pub struct ChatExportBatch {
    /// Newest → oldest within the batch (the `/messages` backward order).
    pub events: Vec<ChatExportEvent>,
    /// Token to pass as `from_token` on the next call. Empty when done.
    pub next_token: String,
    /// Whether the start of the room's history was reached.
    pub reached_start: bool,
}

/// Fetch one backward batch of a room's history for export.
///
/// Events come back decrypted where keys exist (missing historical keys are
/// requested from key backup on demand by the SDK); undecryptable events are
/// returned as `unable_to_decrypt` items rather than dropped. The SDK's HTTP
/// layer already retries 429 rate limits honoring the server's `retry_after`.
///
/// Side effect: `Room::messages` also persists each fetched event into the
/// SQLite event cache, so a full-history export grows the local store.
pub async fn fetch_chat_export_batch(
    handle_id: u64,
    room_id: &str,
    from_token: &str,
    limit: u32,
) -> Result<ChatExportBatch, String> {
    ensure_handle_auth_usable(handle_id)?;
    let client = client_for_handle(handle_id)?;
    let room = joined_room_for_handle(handle_id, room_id)?;
    let own_user_id = client
        .user_id()
        .ok_or_else(|| "matrix-sdk backend runtime handle has no user id".to_owned())?
        .to_owned();

    let mut options = MessagesOptions::backward();
    options.limit = UInt::from(limit);
    if !from_token.is_empty() {
        options.from = Some(from_token.to_owned());
    }

    let response = room
        .messages(options)
        .await
        .map_err(|e| format!("failed to fetch room history for export: {e}"))?;

    let mut events = Vec::with_capacity(response.chunk.len());
    for event in &response.chunk {
        if let Some(export_event) = convert_export_event(event, &room, &own_user_id).await {
            events.push(export_event);
        }
    }

    let reached_start = response.end.is_none();
    Ok(ChatExportBatch {
        events,
        next_token: response.end.unwrap_or_default(),
        reached_start,
    })
}

async fn convert_export_event(
    event: &TimelineEvent,
    room: &Room,
    own_user_id: &matrix_sdk::ruma::UserId,
) -> Option<ChatExportEvent> {
    let mut item = thread_timeline::raw_event_to_timeline_item(event, room, own_user_id).await?;

    let raw = event.raw();

    // The generic summarizer only produces placeholder bodies for state
    // events; overlay the richer raw-path state summary so C++
    // `StateEventText::translate` can render real sentences.
    if let Ok(AnySyncTimelineEvent::State(state_event)) = raw.deserialize() {
        let s = event_summary::summarize_sync_state_event(&state_event);
        item.item_kind = s.kind;
        item.matrix_event_type = s.matrix_event_type;
        item.membership_change_kind = s.membership_change_kind;
        item.body = s.body;
        item.state_event_target_user = s.state_event_target_user;
        item.state_event_target_user_id = s.state_event_target_user_id;
        item.state_event_detail = s.state_event_detail;
        item.state_event_reason = s.state_event_reason;
        item.state_event_has_sender = s.state_event_has_sender;
        item.tombstone_replacement_room_id = s.tombstone_replacement_room_id;
    }

    let raw_json = raw.json().get();
    let (relation_kind, relates_to_event_id, annotation_key) =
        extract_export_relation(raw_json);

    // The summarizer already substitutes the bundled `m.replace` body
    // (MSC2675) but leaves the flag unset on the raw path.
    if has_bundled_edit(raw_json) {
        item.is_edited = true;
    }

    Some(ChatExportEvent { item, relation_kind, relates_to_event_id, annotation_key })
}

/// Classify an event's `content.m.relates_to` for export aggregation:
/// reactions (`m.annotation`) and edits (`m.replace`) are folded into their
/// target events by the C++ formatter instead of being rendered standalone.
fn extract_export_relation(json_str: &str) -> (String, String, String) {
    let parsed: serde_json::Value = match serde_json::from_str(json_str) {
        Ok(v) => v,
        Err(_) => return (String::new(), String::new(), String::new()),
    };

    let relates_to = match parsed.get("content").and_then(|c| c.get("m.relates_to")) {
        Some(r) => r,
        None => return (String::new(), String::new(), String::new()),
    };

    let target = relates_to
        .get("event_id")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned();

    match relates_to.get("rel_type").and_then(|v| v.as_str()) {
        Some("m.annotation") if !target.is_empty() => {
            let key = relates_to
                .get("key")
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_owned();
            ("annotation".to_owned(), target, key)
        }
        Some("m.replace") if !target.is_empty() => ("replacement".to_owned(), target, String::new()),
        _ => (String::new(), String::new(), String::new()),
    }
}

/// Whether the server bundled a later `m.replace` aggregation
/// (`unsigned["m.relations"]["m.replace"]`) into this event.
fn has_bundled_edit(json_str: &str) -> bool {
    let parsed: serde_json::Value = match serde_json::from_str(json_str) {
        Ok(v) => v,
        Err(_) => return false,
    };

    parsed
        .get("unsigned")
        .and_then(|u| u.get("m.relations"))
        .and_then(|r| r.get("m.replace"))
        .is_some()
}

#[cfg(test)]
mod tests {
    use super::{extract_export_relation, has_bundled_edit};

    #[test]
    fn classifies_reaction_relation() {
        let json = r#"{
            "type": "m.reaction",
            "content": {
                "m.relates_to": {
                    "rel_type": "m.annotation",
                    "event_id": "$target",
                    "key": "👍"
                }
            }
        }"#;
        let (kind, target, key) = extract_export_relation(json);
        assert_eq!(kind, "annotation");
        assert_eq!(target, "$target");
        assert_eq!(key, "👍");
    }

    #[test]
    fn classifies_replacement_relation() {
        let json = r#"{
            "type": "m.room.message",
            "content": {
                "body": " * edited",
                "m.relates_to": { "rel_type": "m.replace", "event_id": "$target" }
            }
        }"#;
        let (kind, target, key) = extract_export_relation(json);
        assert_eq!(kind, "replacement");
        assert_eq!(target, "$target");
        assert_eq!(key, "");
    }

    #[test]
    fn replies_are_not_relations_for_export() {
        let json = r#"{
            "type": "m.room.message",
            "content": {
                "body": "a reply",
                "m.relates_to": { "m.in_reply_to": { "event_id": "$target" } }
            }
        }"#;
        let (kind, target, key) = extract_export_relation(json);
        assert_eq!(kind, "");
        assert_eq!(target, "");
        assert_eq!(key, "");
    }

    #[test]
    fn detects_bundled_edit() {
        let json = r#"{
            "type": "m.room.message",
            "content": { "body": "original" },
            "unsigned": {
                "m.relations": {
                    "m.replace": { "event_id": "$edit", "type": "m.room.message" }
                }
            }
        }"#;
        assert!(has_bundled_edit(json));
        assert!(!has_bundled_edit(r#"{"content": {"body": "plain"}}"#));
    }
}
