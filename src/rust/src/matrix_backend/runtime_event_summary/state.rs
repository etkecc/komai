// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Summarize state events: membership changes, profile changes, room
//! name/topic/join-rule/etc. transitions.

use super::*;

pub(super) fn summarize_membership_change(
    change: &RoomMembershipChange,
    sender_display_name: &str,
) -> MatrixEventSummary {
    let user = human_name(change.display_name().as_deref(), change.user_id().as_str());
    let membership_change_kind = membership_change_kind_key(change.change());

    // Whether the sender is a distinct person from the target user.
    let has_sender = !sender_display_name.is_empty() && sender_display_name != user;

    // Extract reason from the membership event content, if available.
    let reason = match change.content() {
        StateEventContentChange::Original { content, .. } => content
            .reason
            .as_deref()
            .filter(|r: &&str| !r.is_empty())
            .map(truncate_reason),
        _ => None,
    };

    let mut s = summary("membership_change", "m.room.member", "");
    s.membership_change_kind = membership_change_kind.to_owned();
    s.state_event_target_user = user;
    s.state_event_target_user_id = change.user_id().to_string();
    s.state_event_reason = reason.unwrap_or_default();
    s.state_event_has_sender = has_sender;
    s
}

pub(super) fn truncate_reason(reason: &str) -> String {
    if reason.chars().count() > 200 {
        let truncated: String = reason.chars().take(200).collect();
        format!("{truncated}…")
    } else {
        reason.to_owned()
    }
}

pub(super) fn summarize_profile_change(change: &MemberProfileChange) -> MatrixEventSummary {
    let user = human_name(None, change.user_id().as_str());

    // Determine the sub-kind of profile change and extract detail if applicable.
    let (detail_key, detail_value) = if let Some(displayname_change) = change.displayname_change() {
        if let Some(new_name) = displayname_change.new.as_deref().filter(|name| !name.is_empty()) {
            ("displayname", new_name.to_owned())
        } else {
            ("displayname_removed", String::new())
        }
    } else if change.avatar_url_change().is_some() {
        ("avatar", String::new())
    } else {
        ("profile", String::new())
    };

    let mut s = summary("profile_change", "m.room.member", "");
    s.state_event_target_user = user;
    s.state_event_target_user_id = change.user_id().to_string();
    s.membership_change_kind = detail_key.to_owned();
    s.state_event_detail = detail_value;
    s
}

pub(super) fn summarize_other_state(state: &OtherState, _sender: &str) -> MatrixEventSummary {
    let event_type = state.content().event_type().to_string();

    // Extract a machine-readable detail key for state changes that have variants.
    // The C++ side maps (matrix_event_type, state_event_detail) to translated text.
    let detail = match state.content() {
        AnyOtherStateEventContentChange::RoomName(content) => match content {
            StateEventContentChange::Original { content, .. } if !content.name.is_empty() => {
                content.name.clone()
            }
            _ => String::new(),
        },
        AnyOtherStateEventContentChange::RoomTopic(content) => match content {
            StateEventContentChange::Original { content, .. } if !content.topic.is_empty() => {
                content.topic.clone()
            }
            _ => String::new(),
        },
        AnyOtherStateEventContentChange::RoomJoinRules(content) => match content {
            StateEventContentChange::Original { content, .. } => match &content.join_rule {
                JoinRule::Invite => "invite".to_owned(),
                JoinRule::Knock => "knock".to_owned(),
                JoinRule::Public => "public".to_owned(),
                JoinRule::Private => "private".to_owned(),
                JoinRule::Restricted(_) => "restricted".to_owned(),
                JoinRule::KnockRestricted(_) => "knock_restricted".to_owned(),
                _ => String::new(),
            },
            _ => String::new(),
        },
        AnyOtherStateEventContentChange::RoomHistoryVisibility(content) => match content {
            StateEventContentChange::Original { content, .. } => {
                match content.history_visibility {
                    HistoryVisibility::Invited => "invited".to_owned(),
                    HistoryVisibility::Joined => "joined".to_owned(),
                    HistoryVisibility::Shared => "shared".to_owned(),
                    HistoryVisibility::WorldReadable => "world_readable".to_owned(),
                    _ => String::new(),
                }
            }
            _ => String::new(),
        },
        AnyOtherStateEventContentChange::RoomGuestAccess(content) => match content {
            StateEventContentChange::Original { content, .. } => match content.guest_access {
                GuestAccess::CanJoin => "can_join".to_owned(),
                GuestAccess::Forbidden => "forbidden".to_owned(),
                _ => String::new(),
            },
            _ => String::new(),
        },
        _ => String::new(),
    };

    // Extract power level user changes for enriched messages.
    // Translated in C++ `StateEventText::translatePowerLevels()`.
    let power_level_changes = match state.content() {
        AnyOtherStateEventContentChange::RoomPowerLevels(StateEventContentChange::Original {
            content,
            prev_content: Some(prev),
        }) => super::super::event_detail::diff_power_level_users(content, prev),
        _ => Vec::new(),
    };

    // Extract server ACL changes for enriched messages.
    // Translated in C++ `StateEventText::translateServerAcl()`.
    let server_acl_changes = match state.content() {
        AnyOtherStateEventContentChange::RoomServerAcl(StateEventContentChange::Original {
            content,
            prev_content: Some(prev),
        }) => {
            let change = super::super::event_detail::diff_server_acl(content, prev);
            if change.is_empty() { None } else { Some(change) }
        }
        _ => None,
    };

    // Tombstone events carry the successor room id in `content.replacement_room`,
    // surfaced here so the "Go to replacement room" button in the timeline
    // delegate can target it directly.
    let tombstone_replacement_room_id = match state.content() {
        AnyOtherStateEventContentChange::RoomTombstone(StateEventContentChange::Original {
            content,
            ..
        }) => content.replacement_room.to_string(),
        _ => String::new(),
    };

    let mut s = summary("other_state", &event_type, "");
    s.state_event_detail = detail;
    s.power_level_changes = power_level_changes;
    s.server_acl_changes = server_acl_changes;
    s.tombstone_replacement_room_id = tombstone_replacement_room_id;
    s
}

/// Summarize a raw-path (`AnySyncStateEvent`) state event with the same kind
/// keys and detail strings the timeline-path summarizers produce, so C++
/// `StateEventText::translate` renders the same sentences for both.
///
/// Unlike the timeline path this works on plain ruma events (no
/// matrix-sdk-ui aggregation), so power-level and server-ACL diffs are not
/// computed; those events fall back to the generic `other_state` rendering.
pub fn summarize_sync_state_event(event: &AnySyncStateEvent) -> MatrixEventSummary {
    use matrix_sdk::ruma::events::SyncStateEvent;
    use matrix_sdk::ruma::events::room::member::MembershipChange as RumaMembershipChange;

    match event {
        AnySyncStateEvent::RoomMember(SyncStateEvent::Original(e)) => {
            let change = e.content.membership_change(
                e.unsigned.prev_content.as_ref().map(|c| c.details()),
                &e.sender,
                &e.state_key,
            );

            if let RumaMembershipChange::ProfileChanged { displayname_change, avatar_url_change } =
                &change
            {
                let (detail_key, detail_value) = if let Some(name_change) = displayname_change {
                    match name_change.new.map(str::trim).filter(|name| !name.is_empty()) {
                        Some(new_name) => ("displayname", new_name.to_owned()),
                        None => ("displayname_removed", String::new()),
                    }
                } else if avatar_url_change.is_some() {
                    ("avatar", String::new())
                } else {
                    ("profile", String::new())
                };

                let mut s = summary("profile_change", "m.room.member", "");
                s.state_event_target_user =
                    human_name(e.content.displayname.as_deref(), e.state_key.as_str());
                s.state_event_target_user_id = e.state_key.to_string();
                s.membership_change_kind = detail_key.to_owned();
                s.state_event_detail = detail_value;
                return s;
            }

            let mut s = summary("membership_change", "m.room.member", "");
            s.membership_change_kind = raw_membership_change_kind_key(&change).to_owned();
            s.state_event_target_user =
                human_name(e.content.displayname.as_deref(), e.state_key.as_str());
            s.state_event_target_user_id = e.state_key.to_string();
            s.state_event_reason = e
                .content
                .reason
                .as_deref()
                .filter(|r| !r.is_empty())
                .map(truncate_reason)
                .unwrap_or_default();
            s.state_event_has_sender = e.sender.as_str() != e.state_key.as_str();
            s
        }
        AnySyncStateEvent::RoomMember(SyncStateEvent::Redacted(e)) => {
            let mut s = summary("membership_change", "m.room.member", "");
            s.membership_change_kind = "redacted".to_owned();
            s.state_event_target_user = e.state_key.to_string();
            s.state_event_target_user_id = e.state_key.to_string();
            s
        }
        AnySyncStateEvent::RoomName(SyncStateEvent::Original(e)) => {
            let mut s = summary("other_state", "m.room.name", "");
            s.state_event_detail = e.content.name.trim().to_owned();
            s
        }
        AnySyncStateEvent::RoomTopic(SyncStateEvent::Original(e)) => {
            let mut s = summary("other_state", "m.room.topic", "");
            s.state_event_detail = e.content.topic.clone();
            s
        }
        AnySyncStateEvent::RoomJoinRules(SyncStateEvent::Original(e)) => {
            let mut s = summary("other_state", "m.room.join_rules", "");
            s.state_event_detail = match &e.content.join_rule {
                JoinRule::Invite => "invite".to_owned(),
                JoinRule::Knock => "knock".to_owned(),
                JoinRule::Public => "public".to_owned(),
                JoinRule::Private => "private".to_owned(),
                JoinRule::Restricted(_) => "restricted".to_owned(),
                JoinRule::KnockRestricted(_) => "knock_restricted".to_owned(),
                _ => String::new(),
            };
            s
        }
        AnySyncStateEvent::RoomHistoryVisibility(SyncStateEvent::Original(e)) => {
            let mut s = summary("other_state", "m.room.history_visibility", "");
            s.state_event_detail = match e.content.history_visibility {
                HistoryVisibility::Invited => "invited".to_owned(),
                HistoryVisibility::Joined => "joined".to_owned(),
                HistoryVisibility::Shared => "shared".to_owned(),
                HistoryVisibility::WorldReadable => "world_readable".to_owned(),
                _ => String::new(),
            };
            s
        }
        AnySyncStateEvent::RoomGuestAccess(SyncStateEvent::Original(e)) => {
            let mut s = summary("other_state", "m.room.guest_access", "");
            s.state_event_detail = match e.content.guest_access {
                GuestAccess::CanJoin => "can_join".to_owned(),
                GuestAccess::Forbidden => "forbidden".to_owned(),
                _ => String::new(),
            };
            s
        }
        AnySyncStateEvent::RoomTombstone(SyncStateEvent::Original(e)) => {
            let mut s = summary("other_state", "m.room.tombstone", "");
            s.tombstone_replacement_room_id = e.content.replacement_room.to_string();
            s
        }
        _ => {
            let event_type = event.event_type().to_string();
            summary("other_state", &event_type, "")
        }
    }
}

fn raw_membership_change_kind_key(
    change: &matrix_sdk::ruma::events::room::member::MembershipChange<'_>,
) -> &'static str {
    use matrix_sdk::ruma::events::room::member::MembershipChange as C;

    match change {
        C::Joined => "joined",
        C::Left => "left",
        C::Banned => "banned",
        C::Unbanned => "unbanned",
        C::Kicked => "kicked",
        C::Invited => "invited",
        C::KickedAndBanned => "kicked_and_banned",
        C::InvitationAccepted => "invitation_accepted",
        C::InvitationRejected => "invitation_rejected",
        C::InvitationRevoked => "invitation_revoked",
        C::Knocked => "knocked",
        C::KnockAccepted => "knock_accepted",
        C::KnockRetracted => "knock_retracted",
        C::KnockDenied => "knock_denied",
        C::None => "none",
        C::Error => "error",
        // ProfileChanged is handled by the caller before reaching here.
        _ => "not_implemented",
    }
}

pub(super) fn human_name(display_name: Option<&str>, user_id: &str) -> String {
    display_name
        .map(str::trim)
        .filter(|name| !name.is_empty())
        .unwrap_or(user_id)
        .to_owned()
}

pub(super) fn membership_change_kind_key(change: Option<MembershipChange>) -> &'static str {
    match change {
        Some(MembershipChange::Joined) => "joined",
        Some(MembershipChange::Left) => "left",
        Some(MembershipChange::Banned) => "banned",
        Some(MembershipChange::Unbanned) => "unbanned",
        Some(MembershipChange::Kicked) => "kicked",
        Some(MembershipChange::Invited) => "invited",
        Some(MembershipChange::KickedAndBanned) => "kicked_and_banned",
        Some(MembershipChange::InvitationAccepted) => "invitation_accepted",
        Some(MembershipChange::InvitationRejected) => "invitation_rejected",
        Some(MembershipChange::InvitationRevoked) => "invitation_revoked",
        Some(MembershipChange::Knocked) => "knocked",
        Some(MembershipChange::KnockAccepted) => "knock_accepted",
        Some(MembershipChange::KnockRetracted) => "knock_retracted",
        Some(MembershipChange::KnockDenied) => "knock_denied",
        Some(MembershipChange::None) => "none",
        Some(MembershipChange::Error) => "error",
        Some(MembershipChange::NotImplemented) => "not_implemented",
        None => "redacted",
    }
}
