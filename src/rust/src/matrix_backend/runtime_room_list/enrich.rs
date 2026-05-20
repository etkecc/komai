// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Per-row enrichment for the room-list snapshot: avatar URL resolution,
//! tag fetch, notification-mode lookup, and thread-receipt math that
//! suppresses room badges when the user has caught up inside a thread.

use super::*;
use super::classify::RoomHeroCandidate;


pub(super) fn direct_chat_avatar_url(
    hero_candidates: &[RoomHeroCandidate],
    partner_user_id: &str,
) -> Option<String> {
    hero_candidates
        .iter()
        .find(|candidate| candidate.user_id == partner_user_id)
        .and_then(|candidate| (!candidate.avatar_url.is_empty()).then(|| candidate.avatar_url.clone()))
}

// The member-lookup branch exists because the homeserver omits the heroes
// summary for rooms that carry an explicit name, leaving DM rooms without a
// room avatar with no server-provided way to find the partner's avatar.
pub(super) async fn resolve_room_avatar_url(
    room: &RoomListItem,
    classification: &MatrixRoomClassification,
    hero_candidates: &[RoomHeroCandidate],
) -> String {
    if let Some(url) = room.avatar_url() {
        return normalize_mxc_uri(url.to_string());
    }

    if !classification.is_direct || classification.direct_chat_other_user_id.is_empty() {
        return String::new();
    }

    if let Some(url) =
        direct_chat_avatar_url(hero_candidates, &classification.direct_chat_other_user_id)
    {
        return url;
    }

    let Ok(partner_user_id) =
        matrix_sdk::ruma::UserId::parse(&classification.direct_chat_other_user_id)
    else {
        return String::new();
    };

    match room.get_member_no_sync(&partner_user_id).await {
        Ok(Some(member)) => member
            .avatar_url()
            .map(|uri| normalize_mxc_uri(uri.to_string()))
            .unwrap_or_default(),
        Ok(None) | Err(_) => String::new(),
    }
}

pub(super) async fn fetch_room_tags(room: &RoomListItem) -> Vec<String> {
    let Ok(tags) = room.tags().await else {
        tracing::debug!(
            room_id = %room.room_id(),
            "Failed to fetch matrix room tags for room-list summary"
        );
        return Vec::new();
    };

    let Some(tags) = tags else {
        return Vec::new();
    };

    let mut tag_ids = tags.keys().map(ToString::to_string).collect::<Vec<_>>();
    tag_ids.sort();
    tag_ids.dedup();
    tag_ids
}


pub(super) async fn user_defined_mode_for(
    notification_settings: &NotificationSettings,
    room: &RoomListItem,
) -> Option<RoomNotificationMode> {
    notification_settings
        .get_user_defined_room_notification_mode(room.room_id())
        .await
}

pub(super) fn apply_notification_mode_to_unread(
    unread_messages: u64,
    unread_mentions: u64,
    user_defined_mode: Option<RoomNotificationMode>,
) -> u64 {
    match user_defined_mode {
        Some(RoomNotificationMode::Mute) => 0,
        Some(RoomNotificationMode::MentionsAndKeywordsOnly) => unread_mentions,
        Some(RoomNotificationMode::AllMessages) | None => unread_messages,
    }
}

/// Extract the thread root id from a raw timeline event, when the event is a
/// thread reply (`content.m.relates_to.rel_type == "m.thread"`).
pub(super) fn extract_thread_root(raw: &Raw<AnySyncTimelineEvent>) -> Option<OwnedEventId> {
    #[derive(serde::Deserialize)]
    struct Content {
        #[serde(rename = "m.relates_to")]
        relates_to: Option<RelatesTo>,
    }
    #[derive(serde::Deserialize)]
    struct RelatesTo {
        rel_type: Option<String>,
        event_id: Option<OwnedEventId>,
    }

    raw.get_field::<Content>("content")
        .ok()
        .flatten()
        .and_then(|c| c.relates_to)
        .and_then(|r| match r.rel_type.as_deref() {
            Some("m.thread") => r.event_id,
            _ => None,
        })
}

/// Returns true when one of our own read receipts (public or private) on the
/// given thread points at `latest_event_id`.
///
/// matrix-sdk-base computes `num_unread_messages` from main/unthreaded
/// receipts only, so a thread-aware client like Element X reading inside a
/// thread leaves the room badge stuck even after the user has caught up. This
/// helper inspects the thread receipt for the thread that contains the room's
/// most recent event — the only one that can be "ahead" of the main marker —
/// and lets the caller suppress the count when it matches.
pub(super) async fn own_thread_receipt_covers(
    room: &RoomListItem,
    thread_root: OwnedEventId,
    latest_event_id: &str,
) -> bool {
    let user_id = room.own_user_id();
    for receipt_type in [ReceiptType::Read, ReceiptType::ReadPrivate] {
        match room
            .load_user_receipt(receipt_type, ReceiptThread::Thread(thread_root.clone()), user_id)
            .await
        {
            Ok(Some((event_id, _))) if event_id.as_str() == latest_event_id => return true,
            _ => {}
        }
    }
    false
}
