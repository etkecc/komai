// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Build the flat `MatrixRoomSummary` snapshot fed back to the C++ side.
//! Pulls per-room enrichment (avatars, tags, notification mode, thread
//! receipts) from sibling helpers.

use super::*;
use super::classify::{classify_room, room_hero_candidates};
use super::enrich::{
    apply_notification_mode_to_unread, extract_thread_root, fetch_room_tags,
    own_thread_receipt_covers, resolve_room_avatar_url, user_defined_mode_for,
};


pub(super) async fn build_room_list_snapshot(
    values: &Vector<RoomListItem>,
    notification_settings: &NotificationSettings,
) -> Vec<MatrixRoomSummary> {
    let mut snapshot = Vec::with_capacity(values.len());
    for room in values.iter() {
        snapshot.push(room_list_item_to_summary(room, notification_settings).await);
    }

    // Populate parent_space_room_ids by reading m.space.child state events
    // from each space. m.space.child on the parent space is the authoritative
    // direction of the relationship; we deliberately don't also consult
    // `Room::parent_spaces()` (which scans m.space.parent on the child) because
    // it offers no extra signal for joined parents and produces noisy upstream
    // logs on the spec-valid "withdrawn" form (content lacking `via`).
    let mut child_to_parents: HashMap<String, Vec<String>> = HashMap::new();
    for room in values.iter() {
        if !room.is_space() {
            continue;
        }
        let space_id = room.room_id().to_string();
        if let Ok(child_events) = room.get_state_events_static::<SpaceChildEventContent>().await {
            for raw_event in child_events {
                let child_room_id: Option<String> = match raw_event.deserialize() {
                    Ok(SyncOrStrippedState::Sync(SyncStateEvent::Original(e))) => {
                        Some(e.state_key.to_string())
                    }
                    Ok(SyncOrStrippedState::Stripped(e)) => Some(e.state_key.to_string()),
                    _ => None,
                };
                if let Some(child_room_id) = child_room_id {
                    child_to_parents
                        .entry(child_room_id)
                        .or_default()
                        .push(space_id.clone());
                }
            }
        }
    }

    for summary in &mut snapshot {
        if let Some(extra_parents) = child_to_parents.remove(&summary.room_id) {
            for parent_id in extra_parents {
                if !summary.parent_space_room_ids.contains(&parent_id) {
                    summary.parent_space_room_ids.push(parent_id);
                }
            }
            summary.parent_space_room_ids.sort();
            summary.parent_space_room_ids.dedup();
        }
    }

    snapshot
}

pub(super) async fn load_ignored_user_ids(client: &Client) -> Vec<String> {
    match client
        .account()
        .account_data::<IgnoredUserListEventContent>()
        .await
    {
        Ok(Some(raw_content)) => match raw_content.deserialize() {
            Ok(content) => {
                let mut user_ids = content
                    .ignored_users
                    .into_keys()
                    .map(|user_id| user_id.to_string())
                    .collect::<Vec<_>>();
                user_ids.sort();
                user_ids.dedup();
                user_ids
            }
            Err(error) => {
                tracing::warn!(%error, "Failed to deserialize matrix-sdk ignored-user list");
                Vec::new()
            }
        },
        Ok(None) => Vec::new(),
        Err(error) => {
            tracing::warn!(%error, "Failed to load matrix-sdk ignored-user list");
            Vec::new()
        }
    }
}



pub(super) async fn room_list_item_to_summary(
    room: &RoomListItem,
    notification_settings: &NotificationSettings,
) -> MatrixRoomSummary {
    let room_state = room.state();
    let hero_candidates = room_hero_candidates(room);
    let classification = classify_room(room, &hero_candidates);
    let avatar_url =
        resolve_room_avatar_url(room, &classification, &hero_candidates).await;
    // `matrix_sdk_base::Room::latest_event` is a synchronous inherent method
    // that returns a `BaseLatestEventValue` populated directly from the room's
    // cached `RoomInfo`. We call it via UFCS to avoid dispatching to
    // `matrix_sdk_ui::timeline::RoomExt::latest_event`, which is async and
    // recomputes the value from the event cache.
    let latest_event: BaseLatestEventValue = matrix_sdk_base::Room::latest_event(room);
    let remote_latest_event = match &latest_event {
        BaseLatestEventValue::Remote(event) => Some(event),
        _ => None,
    };
    let latest_preview = remote_latest_event.and_then(|event| {
        let raw_event: Raw<AnySyncTimelineEvent> = event.raw().clone();
        let event = raw_event.deserialize().ok()?;
        summarize_sync_timeline_event(&event)
    });
    // `latest_event_timestamp()` unifies what used to be two separate sources
    // (sliding sync's latest event vs. the event cache's latest timestamp).
    let timestamp = room
        .latest_event_timestamp()
        .map(|ts| u64::from(ts.get()))
        .unwrap_or_default();
    let latest_event_id = latest_event
        .event_id()
        .map(|id| id.to_string())
        .unwrap_or_default();
    let (last_message_sender_id, last_message_sender_display_name) = match remote_latest_event {
        Some(event) => {
            let sender_id = event
                .raw()
                .get_field::<OwnedUserId>("sender")
                .ok()
                .flatten();
            let mut sender_display_name = String::new();

            if let Some(sender_id) = sender_id.as_ref() {
                match room.get_member_no_sync(sender_id).await {
                    Ok(Some(member)) => {
                        sender_display_name = member
                            .display_name()
                            .map(str::trim)
                            .filter(|name| !name.is_empty())
                            .map(ToOwned::to_owned)
                            .unwrap_or_default();
                    }
                    Ok(None) | Err(_) => {}
                }
            }
            (
                sender_id.map(|user_id| user_id.to_string()).unwrap_or_default(),
                sender_display_name,
            )
        }
        None => (String::new(), String::new()),
    };
    let tags = fetch_room_tags(room).await;
    // Populated by the post-pass in `build_room_list_snapshot` from the
    // authoritative `m.space.child` direction.
    let parent_space_room_ids: Vec<String> = Vec::new();

    // matrix-sdk-base only honours main/unthreaded receipts when computing
    // unread counts. When a thread-aware client (e.g. Element X) reads a
    // thread on another device it sends a thread-scoped receipt instead, so
    // the room badge stays "stuck" here even though the user is caught up.
    // If `latest_event` is itself a thread reply and we have a receipt from
    // our own user covering it on that thread, treat the room as read.
    let mut raw_unread_messages = room.num_unread_messages();
    let mut raw_unread_notifications = room.num_unread_notifications();
    let mut raw_unread_mentions = room.num_unread_mentions();
    if raw_unread_messages > 0
        && let Some(event) = remote_latest_event
        && let Some(thread_root) = extract_thread_root(event.raw())
        && own_thread_receipt_covers(room, thread_root, &latest_event_id).await
    {
        raw_unread_messages = 0;
        raw_unread_notifications = 0;
        raw_unread_mentions = 0;
    }

    let is_invite = matches!(room_state, RoomState::Invited);
    let (inviter_user_id, inviter_display_name, inviter_avatar_url, invite_reason) = if is_invite {
        let invite_details = room.invite_details().await.ok();
        let inviter_user_id = invite_details
            .as_ref()
            .and_then(|details| details.inviter.as_ref())
            .map(|inviter| inviter.user_id().to_string())
            .unwrap_or_default();
        let inviter_display_name = invite_details
            .as_ref()
            .and_then(|details| details.inviter.as_ref())
            .map(|inviter| inviter.name().to_owned())
            .filter(|value| !value.trim().is_empty())
            .unwrap_or_default();
        let inviter_avatar_url = invite_details
            .as_ref()
            .and_then(|details| details.inviter.as_ref())
            .and_then(|inviter| inviter.avatar_url())
            .map(|uri| normalize_mxc_uri(uri.to_string()))
            .unwrap_or_default();
        let invite_reason = invite_details
            .as_ref()
            .and_then(|details| details.invitee.event().reason().map(ToOwned::to_owned))
            .unwrap_or_default();
        (inviter_user_id, inviter_display_name, inviter_avatar_url, invite_reason)
    } else {
        (String::new(), String::new(), String::new(), String::new())
    };

    MatrixRoomSummary {
        room_id: room.room_id().to_string(),
        latest_event_id,
        display_name: room
            .cached_display_name()
            .map(|name| name.to_string())
            .or_else(|| room.name())
            .unwrap_or_else(|| room.room_id().to_string()),
        avatar_url,
        topic: room.topic().unwrap_or_default(),
        room_alias: room
            .canonical_alias()
            .map(|alias| alias.to_string())
            .or_else(|| room.alt_aliases().into_iter().next().map(|alias| alias.to_string()))
            .unwrap_or_default(),
        last_message: latest_preview
            .as_ref()
            .map(|preview| preview.body.clone())
            .unwrap_or_default(),
        last_message_kind: latest_preview
            .map(|preview| preview.kind)
            .unwrap_or_default(),
        last_message_sender_id,
        last_message_sender_display_name,
        tags,
        parent_space_room_ids,
        direct_chat_other_user_id: classification.direct_chat_other_user_id,
        is_invite,
        inviter_user_id,
        inviter_display_name,
        inviter_avatar_url,
        invite_reason,
        is_space: room.is_space(),
        is_direct: classification.is_direct,
        is_bot_room: classification.is_bot_room,
        is_encrypted: room.encryption_state().is_encrypted(),
        is_public: matches!(room.join_rule(), Some(JoinRule::Public)),
        member_count: room.active_members_count(),
        // Mute → 0; MentionsOnly → mention count (badge/bold/dot all read
        // `unread_message_count`); AllMessages/None → raw matrix-sdk count.
        unread_message_count: apply_notification_mode_to_unread(
            raw_unread_messages,
            raw_unread_mentions,
            user_defined_mode_for(notification_settings, room).await,
        ),
        notification_count: raw_unread_notifications,
        highlight_count: raw_unread_mentions,
        is_marked_unread: room.is_marked_unread(),
        // MatrixRTC (Element Call) session state, recomputed on every snapshot
        // (which the sync loop rebuilds whenever `m.call.member` state churns),
        // so the room-level "a call is live here" signal stays current.
        has_active_call: room.has_active_room_call(),
        active_call_participant_count: room.active_room_call_participants().len() as u64,
        timestamp,
    }
}
