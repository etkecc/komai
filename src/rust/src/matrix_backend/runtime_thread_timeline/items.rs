// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Convert raw timeline events into MatrixTimelineItem rows; helpers
//! that extract relation metadata and request reply previews on demand.

use super::super::*;
use super::super::event_summary::{summarize_sync_timeline_event, utd_reason_tag};
use super::super::timeline_snapshot::collect_unavailable_reply_event_ids;

use std::collections::HashSet;
use std::sync::Arc;

use matrix_sdk::Room;
use matrix_sdk::deserialized_responses::{TimelineEvent, TimelineEventKind};
use matrix_sdk::ruma::OwnedEventId;
use matrix_sdk_ui::eyeball_im::Vector;
use matrix_sdk_ui::timeline::{Timeline, TimelineItem};


/// Convert a raw `TimelineEvent` into a basic `MatrixTimelineItem`.
/// Produces a functional item with text, sender, media, and timestamps,
/// but without SDK-processed aggregations like reactions or reply previews.
pub(in crate::matrix_backend) async fn raw_event_to_timeline_item(
    event: &TimelineEvent,
    room: &Room,
    own_user_id: &matrix_sdk::ruma::UserId,
) -> Option<MatrixTimelineItem> {
    let event_id = event.event_id()?.to_string();
    let timestamp = event.timestamp.map(|ts| u64::from(ts.0)).unwrap_or(0);

    let raw = event.raw();
    let deserialized = raw.deserialize().ok()?;

    let sender_id = deserialized.sender().to_string();
    let mut summary = summarize_sync_timeline_event(&deserialized)?;

    // Undecryptable events reach the raw path still encrypted: `raw()`
    // yields the `m.room.encrypted` JSON, which the summarizer can only
    // classify as an unsupported message. The decryption verdict lives on
    // the `TimelineEvent` wrapper, so overlay it here.
    if let TimelineEventKind::UnableToDecrypt { utd_info, .. } = &event.kind {
        summary.kind = "unable_to_decrypt".to_owned();
        summary.matrix_event_type = "m.room.encrypted".to_owned();
        summary.body = "[Unable to decrypt message]".to_owned();
        summary.utd_cause = utd_reason_tag(&utd_info.reason, raw).to_owned();
    }

    let (sender_display_name, sender_avatar_url) =
        resolve_member_profile(room, deserialized.sender()).await;

    let raw_json = raw.json().get();
    let (thread_root_id, reply_to_event_id) = extract_relations_from_raw(raw_json);

    let is_own = deserialized.sender() == own_user_id;
    // `unsigned.transaction_id` is only populated by the homeserver on
    // events it returns to the original sender, so only read it for our
    // own events — saves a serde parse pass on everyone else's events.
    let transaction_id = if is_own {
        extract_transaction_id_from_raw(raw_json)
    } else {
        String::new()
    };

    // Raw-path items (thread roots loaded via /relations) lack the
    // EventTimelineItem wrapper, so we can only see the wire-level
    // encryption flag, not the shield state. Compute this before the
    // summary fields get moved into the struct literal below.
    let is_encrypted_event =
        event.encryption_info().is_some() || summary.kind == "unable_to_decrypt";

    Some(MatrixTimelineItem {
        item_id: event_id.clone(),
        event_id,
        transaction_id,
        delivery_state: String::new(),
        send_error: String::new(),
        is_recoverable: false,
        thread_id: thread_root_id,
        is_thread_root: summary.is_thread_root,
        thread_reply_count: 0,
        sender_id,
        sender_display_name,
        sender_avatar_url,
        body: summary.body,
        formatted_body: summary.formatted_body,
        reply_event_id: reply_to_event_id,
        reply_sender_id: String::new(),
        reply_sender_display_name: String::new(),
        reply_item_kind: String::new(),
        reply_matrix_event_type: String::new(),
        reply_body: String::new(),
        reply_formatted_body: String::new(),
        reply_media_url: String::new(),
        reply_thumbnail_url: String::new(),
        reply_file_name: String::new(),
        reply_mime_type: String::new(),
        reply_media_width: 0,
        reply_media_height: 0,
        reply_media_duration_ms: 0,
        reply_media_size_bytes: 0,
        reply_blurhash: String::new(),
        reactions: Vec::new(),
        reactions_summary: String::new(),
        special_effect_names: summary.special_effect_names,
        item_kind: summary.kind,
        membership_change_kind: String::new(),
        matrix_event_type: summary.matrix_event_type,
        is_edited: false,
        media_url: summary.media.as_ref().map(|m| m.media_url.clone()).unwrap_or_default(),
        thumbnail_url: summary.media.as_ref().map(|m| m.thumbnail_url.clone()).unwrap_or_default(),
        file_name: summary.media.as_ref().map(|m| m.file_name.clone()).unwrap_or_default(),
        mime_type: summary.media.as_ref().map(|m| m.mime_type.clone()).unwrap_or_default(),
        media_width: summary.media.as_ref().map(|m| m.media_width).unwrap_or(0),
        media_height: summary.media.as_ref().map(|m| m.media_height).unwrap_or(0),
        media_duration_ms: summary.media.as_ref().map(|m| m.media_duration_ms).unwrap_or(0),
        media_size_bytes: summary.media.as_ref().map(|m| m.media_size_bytes).unwrap_or(0),
        blurhash: summary.media.as_ref().map(|m| m.blurhash.clone()).unwrap_or_default(),
        media_is_encrypted: summary.media.as_ref().map(|m| m.media_is_encrypted).unwrap_or(false),
        thumbnail_is_encrypted: summary.media.as_ref().map(|m| m.thumbnail_is_encrypted).unwrap_or(false),
        is_voice_message: summary.is_voice_message,
        waveform: summary.waveform,
        timestamp,
        is_own,
        state_event_target_user: String::new(),
        state_event_target_user_id: String::new(),
        state_event_detail: String::new(),
        state_event_reason: String::new(),
        state_event_has_sender: false,
        utd_cause: summary.utd_cause,
        is_encrypted_event,
        // Shield tags left empty on the raw path; the UI treats that as
        // "no shield" (verified/clean), which is a safe default for a
        // thread-root preview fetched out-of-band.
        shield_color: String::new(),
        shield_code: String::new(),
        power_level_changes: Vec::new(),
        server_acl_changes: None,
        tombstone_replacement_room_id: summary.tombstone_replacement_room_id,
    })
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Resolve a user's display name and avatar URL from room membership.
pub(super) async fn resolve_member_profile(
    room: &Room,
    user_id: &matrix_sdk::ruma::UserId,
) -> (String, String) {
    match room.get_member_no_sync(user_id).await {
        Ok(Some(member)) => (
            member.display_name().unwrap_or_default().to_owned(),
            member
                .avatar_url()
                .map(|u| normalize_mxc_uri(u.to_string()))
                .unwrap_or_default(),
        ),
        _ => (user_id.to_string(), String::new()),
    }
}

/// Extract thread root ID and reply-to event ID from raw event JSON.
pub(in crate::matrix_backend) fn extract_relations_from_raw(json_str: &str) -> (String, String) {
    let parsed: serde_json::Value = match serde_json::from_str(json_str) {
        Ok(v) => v,
        Err(_) => return (String::new(), String::new()),
    };

    let relates_to = match parsed.get("content").and_then(|c| c.get("m.relates_to")) {
        Some(r) => r,
        None => return (String::new(), String::new()),
    };

    let thread_root_id = if relates_to.get("rel_type").and_then(|v| v.as_str()) == Some("m.thread")
    {
        relates_to
            .get("event_id")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_owned()
    } else {
        String::new()
    };

    let reply_to_event_id = relates_to
        .get("m.in_reply_to")
        .and_then(|r| r.get("event_id"))
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned();

    (thread_root_id, reply_to_event_id)
}

/// Extract the original client-supplied transaction id from a raw event's
/// `unsigned.transaction_id` field. Per the Matrix spec, homeservers populate
/// this on events they return to the original sender after a successful PUT,
/// which lets us reconcile a just-sent local echo with its server-confirmed
/// /relations twin without guessing by sender + body.
pub(super) fn extract_transaction_id_from_raw(json_str: &str) -> String {
    let parsed: serde_json::Value = match serde_json::from_str(json_str) {
        Ok(v) => v,
        Err(_) => return String::new(),
    };

    parsed
        .get("unsigned")
        .and_then(|u| u.get("transaction_id"))
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned()
}

/// Fire-and-forget tasks to fetch reply details for events with unavailable
/// reply content.
pub(super) fn request_reply_details(
    values: &Vector<Arc<TimelineItem>>,
    timeline: &Arc<Timeline>,
    requested: &mut HashSet<OwnedEventId>,
) {
    for event_id in collect_unavailable_reply_event_ids(values) {
        if requested.insert(event_id.clone()) {
            let timeline_clone = timeline.clone();
            tokio::spawn(async move {
                if let Err(e) = timeline_clone.fetch_details_for_event(&event_id).await {
                    tracing::info!("Failed to fetch thread reply details for {}: {e}", event_id);
                }
            });
        }
    }
}
