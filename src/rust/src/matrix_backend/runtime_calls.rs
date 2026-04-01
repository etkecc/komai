// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::ruma::{events::AnySyncMessageLikeEvent, serde::Raw};
use serde_json::Value;

use super::*;

// ---------------------------------------------------------------------------
// Inbound: Rust parses JSON, passes typed structs to C++
// ---------------------------------------------------------------------------

fn required_string(value: &Value, key: &str) -> Option<String> {
    value.get(key)?.as_str().map(|s| s.to_owned())
}

fn optional_string(value: &Value, key: &str) -> String {
    value
        .get(key)
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned()
}

fn optional_u32(value: &Value, key: &str, default: u32) -> u32 {
    value
        .get(key)
        .and_then(|v| v.as_u64())
        .map(|n| n as u32)
        .unwrap_or(default)
}

fn parse_version(value: &Value, default: &str) -> String {
    match value.get("version") {
        Some(v) if v.is_string() => v.as_str().unwrap().to_owned(),
        Some(v) if v.is_number() => {
            v.as_i64()
                .map(|n| n.to_string())
                .unwrap_or_else(|| default.to_owned())
        }
        _ => default.to_owned(),
    }
}

fn parse_session_description(
    value: &Value,
    key: &str,
) -> Option<crate::ffi::MatrixCallSessionDescription> {
    let obj = value.get(key)?;
    let sdp = obj.get("sdp")?.as_str()?.to_owned();
    let sdp_type = obj.get("type")?.as_str()?.to_owned();
    Some(crate::ffi::MatrixCallSessionDescription { sdp, sdp_type })
}

fn parse_candidates(value: &Value) -> Option<Vec<crate::ffi::MatrixCallIceCandidate>> {
    let arr = value.get("candidates")?.as_array()?;
    let mut result = Vec::with_capacity(arr.len());
    for entry in arr {
        let sdp_mid = required_string(entry, "sdpMid")?;
        let sdp_m_line_index = optional_u32(entry, "sdpMLineIndex", 0) as u16;
        let candidate = required_string(entry, "candidate")?;
        result.push(crate::ffi::MatrixCallIceCandidate {
            sdp_mid,
            sdp_m_line_index,
            candidate,
        });
    }
    Some(result)
}

fn try_dispatch_call_event(
    handle_id: u64,
    room_id: &str,
    event_type: &str,
    sender_id: &str,
    event_id: &str,
    content: &Value,
) -> Option<()> {
    match event_type {
        "m.call.invite" => {
            let event = crate::ffi::MatrixCallInviteEvent {
                room_id: room_id.to_owned(),
                sender_id: sender_id.to_owned(),
                event_id: event_id.to_owned(),
                call_id: required_string(content, "call_id")?,
                party_id: optional_string(content, "party_id"),
                version: parse_version(content, "0"),
                lifetime: optional_u32(content, "lifetime", 90000),
                invitee: optional_string(content, "invitee"),
                offer: parse_session_description(content, "offer")?,
            };
            crate::ffi::matrix_notify_call_invite_received(handle_id, event);
        }
        "m.call.candidates" => {
            let event = crate::ffi::MatrixCallCandidatesEvent {
                room_id: room_id.to_owned(),
                sender_id: sender_id.to_owned(),
                event_id: event_id.to_owned(),
                call_id: required_string(content, "call_id")?,
                party_id: optional_string(content, "party_id"),
                version: parse_version(content, "0"),
                candidates: parse_candidates(content)?,
            };
            crate::ffi::matrix_notify_call_candidates_received(handle_id, event);
        }
        "m.call.answer" => {
            let event = crate::ffi::MatrixCallAnswerEvent {
                room_id: room_id.to_owned(),
                sender_id: sender_id.to_owned(),
                event_id: event_id.to_owned(),
                call_id: required_string(content, "call_id")?,
                party_id: optional_string(content, "party_id"),
                version: parse_version(content, "0"),
                answer: parse_session_description(content, "answer")?,
            };
            crate::ffi::matrix_notify_call_answer_received(handle_id, event);
        }
        "m.call.hangup" => {
            let event = crate::ffi::MatrixCallHangUpEvent {
                room_id: room_id.to_owned(),
                sender_id: sender_id.to_owned(),
                event_id: event_id.to_owned(),
                call_id: required_string(content, "call_id")?,
                party_id: optional_string(content, "party_id"),
                version: parse_version(content, "0"),
                reason: optional_string(content, "reason"),
            };
            crate::ffi::matrix_notify_call_hangup_received(handle_id, event);
        }
        "m.call.select_answer" => {
            let event = crate::ffi::MatrixCallSelectAnswerEvent {
                room_id: room_id.to_owned(),
                sender_id: sender_id.to_owned(),
                event_id: event_id.to_owned(),
                call_id: required_string(content, "call_id")?,
                party_id: required_string(content, "party_id")?,
                version: parse_version(content, "1"),
                selected_party_id: required_string(content, "selected_party_id")?,
            };
            crate::ffi::matrix_notify_call_select_answer_received(handle_id, event);
        }
        "m.call.reject" => {
            let event = crate::ffi::MatrixCallRejectEvent {
                room_id: room_id.to_owned(),
                sender_id: sender_id.to_owned(),
                event_id: event_id.to_owned(),
                call_id: required_string(content, "call_id")?,
                party_id: required_string(content, "party_id")?,
                version: parse_version(content, "1"),
            };
            crate::ffi::matrix_notify_call_reject_received(handle_id, event);
        }
        "m.call.negotiate" => {
            let event = crate::ffi::MatrixCallNegotiateEvent {
                room_id: room_id.to_owned(),
                sender_id: sender_id.to_owned(),
                event_id: event_id.to_owned(),
                call_id: required_string(content, "call_id")?,
                party_id: required_string(content, "party_id")?,
                lifetime: optional_u32(content, "lifetime", 90000),
                description: parse_session_description(content, "description")?,
            };
            crate::ffi::matrix_notify_call_negotiate_received(handle_id, event);
        }
        _ => return None,
    }
    Some(())
}

pub(crate) fn install_incoming_call_event_handlers(
    handle_id: u64,
    client: matrix_sdk::Client,
) -> Vec<EventHandlerDropGuard> {
    let room_handle = client.add_event_handler({
        move |raw: Raw<AnySyncMessageLikeEvent>, room: matrix_sdk::Room| async move {
            let Some(value) = serde_json::from_str::<Value>(raw.json().get()).ok() else {
                return;
            };
            let Some(event_type) = value.get("type").and_then(|v| v.as_str()) else {
                return;
            };
            let event_type = event_type.trim();
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
                return;
            }

            let Some(sender_id) = value.get("sender").and_then(|v| v.as_str()) else {
                return;
            };
            let Some(event_id) = value.get("event_id").and_then(|v| v.as_str()) else {
                return;
            };
            if sender_id.is_empty() || event_id.is_empty() {
                return;
            }

            let Some(content) = value.get("content") else {
                return;
            };

            let room_id = room.room_id().to_string();
            tracing::debug!(
                handle_id,
                room_id,
                event_type,
                event_id,
                sender_id,
                "Forwarding incoming matrix-sdk call event to C++ CallManager"
            );

            if try_dispatch_call_event(
                handle_id, &room_id, event_type, sender_id, event_id, content,
            )
            .is_none()
            {
                tracing::warn!(
                    handle_id,
                    room_id,
                    event_type,
                    event_id,
                    "Failed to parse incoming call event content"
                );
            }
        }
    });

    vec![client.event_handler_drop_guard(room_handle)]
}

// ---------------------------------------------------------------------------
// Outbound: C++ passes struct fields, Rust serializes to JSON
// ---------------------------------------------------------------------------

fn serialize_version(json: &mut serde_json::Map<String, Value>, version: &str) {
    if version == "0" {
        json.insert("version".to_owned(), Value::from(0));
    } else {
        json.insert("version".to_owned(), Value::from(version));
    }
}

fn serialize_session_description(sdp: &str, sdp_type: &str) -> Value {
    serde_json::json!({
        "sdp": sdp,
        "type": sdp_type,
    })
}

pub fn serialize_call_invite(
    call_id: &str,
    party_id: &str,
    version: &str,
    lifetime: u32,
    invitee: &str,
    offer_sdp: &str,
    offer_type: &str,
) -> Result<String, String> {
    let mut json = serde_json::Map::new();
    json.insert("call_id".to_owned(), Value::from(call_id));
    json.insert(
        "offer".to_owned(),
        serialize_session_description(offer_sdp, offer_type),
    );
    json.insert("lifetime".to_owned(), Value::from(lifetime));
    serialize_version(&mut json, version);
    if version != "0" {
        json.insert("party_id".to_owned(), Value::from(party_id));
        if !invitee.is_empty() {
            json.insert("invitee".to_owned(), Value::from(invitee));
        }
    }
    serde_json::to_string(&json).map_err(|e| format!("failed to serialize call invite: {e}"))
}

pub fn serialize_call_candidates(
    call_id: &str,
    party_id: &str,
    version: &str,
    candidates: &[crate::ffi::MatrixCallIceCandidate],
) -> Result<String, String> {
    let mut json = serde_json::Map::new();
    json.insert("call_id".to_owned(), Value::from(call_id));
    let candidates_array: Vec<Value> = candidates
        .iter()
        .map(|c| {
            serde_json::json!({
                "sdpMid": c.sdp_mid,
                "sdpMLineIndex": c.sdp_m_line_index,
                "candidate": c.candidate,
            })
        })
        .collect();
    json.insert("candidates".to_owned(), Value::from(candidates_array));
    serialize_version(&mut json, version);
    if version != "0" {
        json.insert("party_id".to_owned(), Value::from(party_id));
    }
    serde_json::to_string(&json)
        .map_err(|e| format!("failed to serialize call candidates: {e}"))
}

pub fn serialize_call_answer(
    call_id: &str,
    party_id: &str,
    version: &str,
    answer_sdp: &str,
    answer_type: &str,
) -> Result<String, String> {
    let mut json = serde_json::Map::new();
    json.insert("call_id".to_owned(), Value::from(call_id));
    json.insert(
        "answer".to_owned(),
        serialize_session_description(answer_sdp, answer_type),
    );
    serialize_version(&mut json, version);
    if version != "0" {
        json.insert("party_id".to_owned(), Value::from(party_id));
    }
    serde_json::to_string(&json).map_err(|e| format!("failed to serialize call answer: {e}"))
}

pub fn serialize_call_hangup(
    call_id: &str,
    party_id: &str,
    version: &str,
    reason: &str,
) -> Result<String, String> {
    let mut json = serde_json::Map::new();
    json.insert("call_id".to_owned(), Value::from(call_id));
    serialize_version(&mut json, version);
    if version != "0" {
        json.insert("party_id".to_owned(), Value::from(party_id));
    }
    // "user" means no reason field (the User variant)
    if !reason.is_empty() && reason != "user" {
        json.insert("reason".to_owned(), Value::from(reason));
    }
    serde_json::to_string(&json).map_err(|e| format!("failed to serialize call hangup: {e}"))
}

pub fn serialize_call_select_answer(
    call_id: &str,
    party_id: &str,
    version: &str,
    selected_party_id: &str,
) -> Result<String, String> {
    let mut json = serde_json::Map::new();
    json.insert("call_id".to_owned(), Value::from(call_id));
    json.insert("party_id".to_owned(), Value::from(party_id));
    json.insert(
        "selected_party_id".to_owned(),
        Value::from(selected_party_id),
    );
    serialize_version(&mut json, version);
    serde_json::to_string(&json)
        .map_err(|e| format!("failed to serialize call select_answer: {e}"))
}

pub fn serialize_call_reject(
    call_id: &str,
    party_id: &str,
    version: &str,
) -> Result<String, String> {
    let mut json = serde_json::Map::new();
    json.insert("call_id".to_owned(), Value::from(call_id));
    json.insert("party_id".to_owned(), Value::from(party_id));
    serialize_version(&mut json, version);
    serde_json::to_string(&json).map_err(|e| format!("failed to serialize call reject: {e}"))
}

pub fn serialize_call_negotiate(
    call_id: &str,
    party_id: &str,
    lifetime: u32,
    description_sdp: &str,
    description_type: &str,
) -> Result<String, String> {
    let mut json = serde_json::Map::new();
    json.insert("call_id".to_owned(), Value::from(call_id));
    json.insert("party_id".to_owned(), Value::from(party_id));
    json.insert("lifetime".to_owned(), Value::from(lifetime));
    json.insert(
        "description".to_owned(),
        serialize_session_description(description_sdp, description_type),
    );
    json.insert("version".to_owned(), Value::from("1"));
    serde_json::to_string(&json).map_err(|e| format!("failed to serialize call negotiate: {e}"))
}
