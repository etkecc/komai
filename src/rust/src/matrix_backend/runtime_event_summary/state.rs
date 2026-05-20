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

    let mut s = summary("other_state", &event_type, "");
    s.state_event_detail = detail;
    s.power_level_changes = power_level_changes;
    s.server_acl_changes = server_acl_changes;
    s
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
