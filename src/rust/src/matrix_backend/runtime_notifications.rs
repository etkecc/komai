// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::BTreeMap;

use matrix_sdk::{
    Room,
    deserialized_responses::RawAnySyncOrStrippedTimelineEvent,
    ruma::{
    EventId, OwnedEventId, OwnedRoomId,
    events::{
        AnyStrippedStateEvent, AnySyncMessageLikeEvent, AnySyncTimelineEvent, TimelineEventType,
        room::{
            member::{MembershipState, StrippedRoomMemberEvent},
            message::{Relation, SyncRoomMessageEvent},
        },
    },
    push::{PredefinedOverrideRuleId, RuleKind},
    },
};
use matrix_sdk_ui::notification_client::{
    NotificationClient, NotificationEvent as SdkNotificationEvent,
    NotificationItemsRequest, NotificationProcessSetup, NotificationStatus,
};

use super::*;
use super::event_summary::{MatrixEventSummary, summarize_sync_timeline_event};
use crate::html_processor::to_notification_markup;

/// Whether a fetched notification item is a MatrixRTC call notification/decline,
/// which is surfaced through its own desktop notification (runtime_rtc) and must
/// not also go through the generic message-notification pipeline.
fn is_rtc_notification_event(
    notification: &matrix_sdk_ui::notification_client::NotificationItem,
) -> bool {
    match &notification.event {
        SdkNotificationEvent::Timeline(event) => matches!(
            event.event_type().to_string().as_str(),
            "m.rtc.notification"
                | "org.matrix.msc4075.rtc.notification"
                | "m.rtc.decline"
                | "org.matrix.msc4310.rtc.decline"
        ),
        _ => false,
    }
}

/// Whether a notification item's kind is a content-less state change
/// (membership join/leave/kick/ban, or a profile display-name/avatar change).
/// These surface only as bracketed "[Membership change]"/"[Profile updated]"
/// placeholders and are pure churn noise, so they never raise a desktop
/// notification. Genuine invites to the user take a separate path
/// (matrix_notify_notification_item_received, kind "invite") and are unaffected.
fn is_suppressed_state_notification_kind(kind: &str) -> bool {
    matches!(kind, "membership_change" | "profile_change")
}

fn notification_event_id(notification: &matrix_sdk::sync::Notification) -> Option<String> {
    match &notification.event {
        RawAnySyncOrStrippedTimelineEvent::Sync(raw_event) => {
            let event: AnySyncTimelineEvent = raw_event.deserialize().ok()?;
            Some(event.event_id().to_string())
        }
        RawAnySyncOrStrippedTimelineEvent::Stripped(_) => None,
    }
}

async fn live_invite_notification_item(
    room: &Room,
    event: &StrippedRoomMemberEvent,
) -> Option<MatrixNotificationItem> {
    if room.state() != matrix_sdk::RoomState::Invited || event.content.membership != MembershipState::Invite {
        return None;
    }

    let room_name = room
        .display_name()
        .await
        .ok()
        .map(|name| name.to_string())
        .filter(|value| !value.trim().is_empty())
        .unwrap_or_else(|| room.room_id().to_string());

    let room_avatar_url = room
        .avatar_url()
        .map(|uri| normalize_mxc_uri(uri.to_string()))
        .unwrap_or_default();

    let invite_details = room.invite_details().await.ok();
    let sender_display_name = invite_details
        .as_ref()
        .and_then(|details| details.inviter.as_ref())
        .map(|inviter| inviter.name().to_owned())
        .filter(|value| !value.trim().is_empty())
        .unwrap_or_else(|| event.sender.to_string());

    let sender_avatar_url = invite_details
        .as_ref()
        .and_then(|details| details.inviter.as_ref())
        .and_then(|inviter| inviter.avatar_url())
        .map(|uri| normalize_mxc_uri(uri.to_string()))
        .unwrap_or_default();

    Some(MatrixNotificationItem {
        room_id: room.room_id().to_string(),
        event_id: String::new(),
        replacement_event_id: String::new(),
        room_name,
        avatar_url: if !room_avatar_url.is_empty() {
            room_avatar_url
        } else {
            sender_avatar_url
        },
        sender_display_name,
        notification_kind: "invite".to_owned(),
        plain_body: String::new(),
        formatted_body: String::new(),
        media_mxc_url: String::new(),
        is_reply: false,
        is_emote: false,
        is_encrypted: false,
        contains_spoiler: false,
        has_inline_image: false,
        play_sound: true,
    })
}

pub async fn install_live_notification_handler(handle_id: u64, client: Client) {
    client
        .register_notification_handler(move |notification, room, _client| async move {
            match &notification.event {
                RawAnySyncOrStrippedTimelineEvent::Sync(raw_event) => {
                    // MatrixRTC call notifications/declines get their own, richer
                    // desktop notification (with Join/Decline) via runtime_rtc;
                    // skip them here so the generic message pipeline does not also
                    // pop a plain "sent a message" notification for the same event.
                    if matches!(
                        raw_event.get_field::<String>("type").ok().flatten().as_deref(),
                        Some(
                            "m.rtc.notification"
                                | "org.matrix.msc4075.rtc.notification"
                                | "m.rtc.decline"
                                | "org.matrix.msc4310.rtc.decline"
                        )
                    ) {
                        return;
                    }

                    let Some(event_id) = notification_event_id(&notification) else {
                        tracing::debug!(
                            handle_id,
                            room_id = %room.room_id(),
                            "Skipping live notification without an event id"
                        );
                        return;
                    };

                    crate::ffi::matrix_notify_notification_received(
                        handle_id,
                        room.room_id().as_str(),
                        &event_id,
                    );
                }
                RawAnySyncOrStrippedTimelineEvent::Stripped(raw_event) => {
                    let Ok(event) = raw_event.deserialize() else {
                        tracing::debug!(
                            handle_id,
                            room_id = %room.room_id(),
                            "Skipping stripped live notification with unexpected event shape"
                        );
                        return;
                    };

                    let AnyStrippedStateEvent::RoomMember(event) = event else {
                        tracing::debug!(
                            handle_id,
                            room_id = %room.room_id(),
                            "Skipping stripped live notification without room-member payload"
                        );
                        return;
                    };

                    let Some(item) = live_invite_notification_item(&room, &event).await else {
                        tracing::debug!(
                            handle_id,
                            room_id = %room.room_id(),
                            "Skipping stripped live notification without invite payload"
                        );
                        return;
                    };

                    crate::ffi::matrix_notify_notification_item_received(
                        handle_id,
                        crate::matrix_backend::ffi::into_ffi_matrix_notification_item(item),
                    );
                }
            }
        })
        .await;
}

fn notification_requests_by_room(
    requests: &[MatrixNotificationRequest],
) -> BTreeMap<OwnedRoomId, Vec<OwnedEventId>> {
    let mut grouped = BTreeMap::<OwnedRoomId, Vec<OwnedEventId>>::new();

    for request in requests {
        let parsed_room_id = match parse_room_id(&request.room_id) {
            Ok(room_id) => room_id,
            Err(error) => {
                tracing::warn!(
                    room_id = %request.room_id,
                    event_id = %request.event_id,
                    %error,
                    "Skipping invalid matrix notification room id"
                );
                continue;
            }
        };

        let trimmed_event_id = request.event_id.trim();
        let parsed_event_id = match EventId::parse(trimmed_event_id) {
            Ok(event_id) => event_id,
            Err(error) => {
                tracing::warn!(
                    room_id = %request.room_id,
                    event_id = %request.event_id,
                    %error,
                    "Skipping invalid matrix notification event id"
                );
                continue;
            }
        };

        grouped.entry(parsed_room_id).or_default().push(parsed_event_id);
    }

    grouped
}

fn replacement_event_id(event: &AnySyncTimelineEvent) -> String {
    match event {
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(
            SyncRoomMessageEvent::Original(event),
        )) => match &event.content.relates_to {
            Some(Relation::Replacement(replacement)) => replacement.event_id.to_string(),
            _ => String::new(),
        },
        _ => String::new(),
    }
}

fn contains_spoiler_markup(formatted_body: &str) -> bool {
    formatted_body.contains("data-mx-spoiler")
}

fn notification_item_from_sdk(
    room_id: &str,
    event_id: &str,
    notification: matrix_sdk_ui::notification_client::NotificationItem,
) -> MatrixNotificationItem {
    let sender_display_name = notification
        .sender_display_name
        .clone()
        .filter(|value| !value.trim().is_empty())
        .unwrap_or_else(|| notification.event.sender().to_string());
    let room_name = notification.room_computed_display_name.clone();
    let avatar_url = notification
        .room_avatar_url
        .clone()
        .or(notification.sender_avatar_url.clone())
        .map(normalize_mxc_uri)
        .unwrap_or_default();

    match &notification.event {
        SdkNotificationEvent::Timeline(event) => {
            let summary = summarize_sync_timeline_event(event.as_ref()).unwrap_or_else(|| {
                MatrixEventSummary {
                    kind: "unknown_message".to_owned(),
                    membership_change_kind: String::new(),
                    matrix_event_type: event.event_type().to_string(),
                    body: String::new(),
                    formatted_body: String::new(),
                    thread_root_id: String::new(),
                    is_thread_root: false,
                    thread_reply_count: 0,
                    reply_event_id: String::new(),
                    reply_sender_id: String::new(),
                    reply_sender_display_name: String::new(),
                    reply_item_kind: String::new(),
                    reply_matrix_event_type: String::new(),
                    reply_body: String::new(),
                    reply_formatted_body: String::new(),
                    reply_media: None,
                    reactions: Vec::new(),
                    reactions_summary: String::new(),
                    special_effect_names: Vec::new(),
                    is_edited: false,
                    media: None,
                    is_voice_message: false,
                    waveform: Vec::new(),
                    state_event_target_user: String::new(),
                    state_event_target_user_id: String::new(),
                    state_event_detail: String::new(),
                    state_event_reason: String::new(),
                    state_event_has_sender: false,
                    utd_cause: String::new(),
                    power_level_changes: Vec::new(),
                    server_acl_changes: None,
                    tombstone_replacement_room_id: String::new(),
                }
            });

            let media_mxc_url = summary
                .media
                .as_ref()
                .map(|media| {
                    if !media.thumbnail_url.is_empty() {
                        media.thumbnail_url.clone()
                    } else {
                        media.media_url.clone()
                    }
                })
                .unwrap_or_default();
            let has_inline_image =
                matches!(summary.kind.as_str(), "image" | "sticker") && !media_mxc_url.is_empty();
            let is_encrypted = matches!(event.event_type(), TimelineEventType::RoomEncrypted)
                               || summary.kind == "unable_to_decrypt";

            MatrixNotificationItem {
                room_id: room_id.to_owned(),
                event_id: event_id.to_owned(),
                replacement_event_id: replacement_event_id(event.as_ref()),
                room_name,
                avatar_url,
                sender_display_name,
                notification_kind: summary.kind.clone(),
                plain_body: summary.body.clone(),
                formatted_body: to_notification_markup(&summary.formatted_body),
                media_mxc_url,
                is_reply: !summary.reply_event_id.is_empty(),
                is_emote: summary.kind == "emote",
                is_encrypted,
                contains_spoiler: contains_spoiler_markup(&summary.formatted_body),
                has_inline_image,
                play_sound: notification.is_noisy.unwrap_or(false),
            }
        }
        SdkNotificationEvent::Invite(_) => MatrixNotificationItem {
            room_id: room_id.to_owned(),
            event_id: event_id.to_owned(),
            replacement_event_id: String::new(),
            room_name,
            avatar_url,
            sender_display_name,
            notification_kind: "invite".to_owned(),
            plain_body: String::new(),
            formatted_body: String::new(),
            media_mxc_url: String::new(),
            is_reply: false,
            is_emote: false,
            is_encrypted: false,
            contains_spoiler: false,
            has_inline_image: false,
            play_sound: notification.is_noisy.unwrap_or(false),
        },
    }
}

pub async fn fetch_notification_items(
    handle_id: u64,
    requests: &[MatrixNotificationRequest],
) -> Result<Vec<MatrixNotificationItem>, String> {
    if requests.is_empty() {
        return Ok(Vec::new());
    }

    let grouped_requests = notification_requests_by_room(requests);
    if grouped_requests.is_empty() {
        return Ok(Vec::new());
    }

    let client = client_for_handle(handle_id)?;
    let notification_client = NotificationClient::new(
        client,
        NotificationProcessSetup::MultipleProcesses,
    )
    .await
    .map_err(|error| format!("failed to create matrix-sdk notification client: {error}"))?;

    let sdk_requests = grouped_requests
        .into_iter()
        .map(|(room_id, event_ids)| NotificationItemsRequest { room_id, event_ids })
        .collect::<Vec<_>>();
    let event_room_ids = sdk_requests
        .iter()
        .flat_map(|request| {
            request
                .event_ids
                .iter()
                .map(|event_id| (event_id.to_string(), request.room_id.to_string()))
        })
        .collect::<BTreeMap<_, _>>();

    let batch_result = notification_client
        .get_notifications(&sdk_requests)
        .await
        .map_err(|error| format!("failed to fetch matrix-sdk notification batch: {error}"))?;

    let mut items = Vec::new();
    for (event_id, status_result) in batch_result {
        let room_id = event_room_ids
            .get(event_id.as_str())
            .cloned()
            .unwrap_or_default();

        match status_result {
            Ok(NotificationStatus::Event(notification)) => {
                // MatrixRTC call notifications get their own Join/Decline desktop
                // notification (runtime_rtc); never surface them through the
                // generic message pipeline (e.g. a pending one fetched at startup).
                if is_rtc_notification_event(&notification) {
                    tracing::debug!(handle_id, %event_id, "Skipping RTC call notification in the message pipeline");
                    continue;
                }
                let item = notification_item_from_sdk(&room_id, event_id.as_str(), *notification);
                if is_suppressed_state_notification_kind(&item.notification_kind) {
                    tracing::debug!(
                        handle_id,
                        %event_id,
                        kind = %item.notification_kind,
                        "Skipping content-less state-change notification"
                    );
                    continue;
                }
                items.push(item);
            }
            Ok(NotificationStatus::EventFilteredOut) => {
                tracing::debug!(handle_id, %event_id, "Skipping filtered-out notification event");
            }
            Ok(NotificationStatus::EventNotFound) => {
                tracing::debug!(handle_id, %event_id, "Notification event was not found");
            }
            Ok(NotificationStatus::EventRedacted) => {
                tracing::debug!(handle_id, %event_id, "Skipping redacted notification event");
            }
            Err(error) => {
                tracing::warn!(
                    handle_id,
                    %event_id,
                    %error,
                    "Failed to resolve matrix-sdk notification event"
                );
            }
        }
    }

    Ok(items)
}

pub async fn fetch_account_notifications_enabled(handle_id: u64) -> Result<bool, String> {
    let client = client_for_handle(handle_id)?;
    let settings = client.notification_settings().await;

    let master_rule_enabled = settings
        .is_push_rule_enabled(RuleKind::Override, PredefinedOverrideRuleId::Master.as_str())
        .await
        .map_err(|error| format!("failed to fetch matrix-sdk master push rule state: {error}"))?;

    Ok(!master_rule_enabled)
}

pub async fn set_account_notifications_enabled(
    handle_id: u64,
    enabled: bool,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let settings = client.notification_settings().await;

    settings
        .set_push_rule_enabled(
            RuleKind::Override,
            PredefinedOverrideRuleId::Master.as_str(),
            !enabled,
        )
        .await
        .map_err(|error| format!("failed to update matrix-sdk master push rule state: {error}"))?;

    Ok(())
}

#[cfg(test)]
mod tests {
    use super::is_suppressed_state_notification_kind;

    #[test]
    fn suppresses_membership_and_profile_change_notifications() {
        assert!(is_suppressed_state_notification_kind("membership_change"));
        assert!(is_suppressed_state_notification_kind("profile_change"));
    }

    #[test]
    fn keeps_invites_and_content_bearing_notifications() {
        // Invites to the user (kind "invite") and real content must still notify.
        for kind in ["invite", "text", "image", "emote", "other_state", "unknown_message"] {
            assert!(
                !is_suppressed_state_notification_kind(kind),
                "kind {kind:?} must not be suppressed"
            );
        }
    }
}
