// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::{
    ruma::{
        events::{
            AnySyncMessageLikeEvent, AnySyncStateEvent, AnySyncTimelineEvent,
            room::message::{MessageType, Relation, SyncRoomMessageEvent},
        },
    },
};
use matrix_sdk_ui::timeline::{
    AnyOtherFullStateEventContent, MemberProfileChange, MembershipChange, MsgLikeKind, OtherState,
    RoomMembershipChange, TimelineItemContent,
};

pub struct MatrixEventSummary {
    pub kind: String,
    pub body: String,
}

pub fn summarize_timeline_content(content: &TimelineItemContent) -> MatrixEventSummary {
    match content {
        TimelineItemContent::MsgLike(content) => match &content.kind {
            MsgLikeKind::Message(message) => summary_from_message_type(message.msgtype()),
            MsgLikeKind::Sticker(_) => summary("sticker", "[Sticker]"),
            MsgLikeKind::Poll(_) => summary("poll", "[Poll]"),
            MsgLikeKind::Redacted => summary("redacted", "[Redacted message]"),
            MsgLikeKind::UnableToDecrypt(_) => {
                summary("unable_to_decrypt", "[Unable to decrypt message]")
            }
            MsgLikeKind::Other(_) => summary("other_message", "[Unsupported message event]"),
        },
        TimelineItemContent::MembershipChange(change) => summarize_membership_change(change),
        TimelineItemContent::ProfileChange(change) => summarize_profile_change(change),
        TimelineItemContent::OtherState(state) => summarize_other_state(state),
        TimelineItemContent::FailedToParseMessageLike { .. } => {
            summary("failed_to_parse_message_like", "[Unreadable message event]")
        }
        TimelineItemContent::FailedToParseState { .. } => {
            summary("failed_to_parse_state", "[Unreadable state event]")
        }
        TimelineItemContent::CallInvite => summary("call_invite", "[Call invite]"),
        TimelineItemContent::RtcNotification => {
            summary("rtc_notification", "[RTC notification]")
        }
    }
}

pub fn summarize_sync_timeline_event(event: &AnySyncTimelineEvent) -> Option<MatrixEventSummary> {
    match event {
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(message)) => {
            Some(summarize_room_message_event(message))
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::Sticker(_)) => {
            Some(summary("sticker", "[Sticker]"))
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::UnstablePollStart(_)) => {
            Some(summary("poll", "[Poll]"))
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::CallInvite(_)) => {
            Some(summary("call_invite", "[Call invite]"))
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RtcNotification(_)) => {
            Some(summary("rtc_notification", "[RTC notification]"))
        }
        AnySyncTimelineEvent::State(AnySyncStateEvent::RoomMember(_)) => {
            Some(summary("membership_change", "[Membership change]"))
        }
        _ => None,
    }
}

fn summarize_room_message_event(message: &SyncRoomMessageEvent) -> MatrixEventSummary {
    match message {
        SyncRoomMessageEvent::Original(event) => {
            if let Some(replacement) = event
                   .unsigned
                   .relations
                   .replace
                   .as_ref()
                   .and_then(|event| match &event.content.relates_to {
                       Some(Relation::Replacement(replacement)) => {
                           Some(replacement.new_content.msgtype.clone())
                       }
                       _ => None,
                   }) {
                return summary_from_message_type(&replacement);
            }

            summary_from_message_type(&event.content.msgtype)
        }
        SyncRoomMessageEvent::Redacted(_) => summary("redacted", "[Redacted message]"),
    }
}

fn summarize_membership_change(change: &RoomMembershipChange) -> MatrixEventSummary {
    let user = human_name(change.display_name().as_deref(), change.user_id().as_str());

    match change.change() {
        Some(MembershipChange::Joined) => {
            summary("membership_change", &format!("{user} joined the room"))
        }
        Some(MembershipChange::Left) => {
            summary("membership_change", &format!("{user} left the room"))
        }
        Some(MembershipChange::Banned) => {
            summary("membership_change", &format!("{user} was banned"))
        }
        Some(MembershipChange::Unbanned) => {
            summary("membership_change", &format!("{user} was unbanned"))
        }
        Some(MembershipChange::Kicked) => {
            summary("membership_change", &format!("{user} was kicked"))
        }
        Some(MembershipChange::Invited) => {
            summary("membership_change", &format!("{user} was invited"))
        }
        Some(MembershipChange::KickedAndBanned) => {
            summary("membership_change", &format!("{user} was kicked and banned"))
        }
        Some(MembershipChange::InvitationAccepted) => {
            summary("membership_change", &format!("{user} accepted the invite"))
        }
        Some(MembershipChange::InvitationRejected) => {
            summary("membership_change", &format!("{user} rejected the invite"))
        }
        Some(MembershipChange::InvitationRevoked) => {
            summary("membership_change", &format!("{user}'s invite was revoked"))
        }
        Some(MembershipChange::Knocked) => {
            summary("membership_change", &format!("{user} requested to join"))
        }
        Some(MembershipChange::KnockAccepted) => {
            summary("membership_change", &format!("{user}'s knock was accepted"))
        }
        Some(MembershipChange::KnockRetracted) => {
            summary("membership_change", &format!("{user} withdrew the join request"))
        }
        Some(MembershipChange::KnockDenied) => {
            summary("membership_change", &format!("{user}'s join request was denied"))
        }
        Some(MembershipChange::None)
        | Some(MembershipChange::Error)
        | Some(MembershipChange::NotImplemented)
        | None => summary("membership_change", &format!("Membership updated for {user}")),
    }
}

fn summarize_profile_change(change: &MemberProfileChange) -> MatrixEventSummary {
    let user = human_name(None, change.user_id().as_str());

    if let Some(displayname_change) = change.displayname_change() {
        if let Some(new_name) = displayname_change.new.as_deref().filter(|name| !name.is_empty()) {
            return summary("profile_change", &format!("{user} is now known as {new_name}"));
        }
    }

    if change.avatar_url_change().is_some() {
        return summary("profile_change", &format!("{user} changed their avatar"));
    }

    summary("profile_change", &format!("{user} updated their profile"))
}

fn summarize_other_state(state: &OtherState) -> MatrixEventSummary {
    match state.content() {
        AnyOtherFullStateEventContent::RoomName(content) => match content {
            matrix_sdk::ruma::events::FullStateEventContent::Original { content, .. }
                if !content.name.is_empty() =>
            {
                summary("other_state", &format!("Room name changed to {}", content.name))
            }
            _ => summary("other_state", "Room name changed"),
        },
        AnyOtherFullStateEventContent::RoomTopic(content) => match content {
            matrix_sdk::ruma::events::FullStateEventContent::Original { content, .. }
                if !content.topic.is_empty() =>
            {
                summary("other_state", &format!("Room topic changed to {}", content.topic))
            }
            _ => summary("other_state", "Room topic changed"),
        },
        AnyOtherFullStateEventContent::RoomAvatar(_) => {
            summary("other_state", "Room avatar changed")
        }
        AnyOtherFullStateEventContent::RoomEncryption(_) => {
            summary("other_state", "Enabled end-to-end encryption")
        }
        AnyOtherFullStateEventContent::RoomPinnedEvents(_) => {
            summary("other_state", "Pinned messages changed")
        }
        AnyOtherFullStateEventContent::RoomPowerLevels(_) => {
            summary("other_state", "Room permissions changed")
        }
        AnyOtherFullStateEventContent::RoomJoinRules(_) => {
            summary("other_state", "Room access rules changed")
        }
        AnyOtherFullStateEventContent::RoomHistoryVisibility(_) => {
            summary("other_state", "Room history visibility changed")
        }
        AnyOtherFullStateEventContent::RoomGuestAccess(_) => {
            summary("other_state", "Room guest access changed")
        }
        AnyOtherFullStateEventContent::RoomCanonicalAlias(_) => {
            summary("other_state", "Room alias changed")
        }
        AnyOtherFullStateEventContent::RoomTombstone(_) => {
            summary("other_state", "Room was replaced")
        }
        other => summary("other_state", &format!("State event: {}", other.event_type())),
    }
}

fn summary_from_message_type(message_type: &MessageType) -> MatrixEventSummary {
    match message_type {
        MessageType::Text(_) => summary("message", message_type.body()),
        MessageType::Notice(_) => summary("notice", message_type.body()),
        MessageType::Emote(_) => summary("emote", message_type.body()),
        MessageType::Image(_) => summary("image", message_type.body()),
        MessageType::Video(_) => summary("video", message_type.body()),
        MessageType::Audio(_) => summary("audio", message_type.body()),
        MessageType::File(_) => summary("file", message_type.body()),
        MessageType::Location(_) => summary("location", message_type.body()),
        _ => summary("message", message_type.body()),
    }
}

fn human_name(display_name: Option<&str>, user_id: &str) -> String {
    display_name
        .map(str::trim)
        .filter(|name| !name.is_empty())
        .unwrap_or(user_id)
        .to_owned()
}

fn summary(kind: &str, body: &str) -> MatrixEventSummary {
    MatrixEventSummary { kind: kind.to_owned(), body: body.to_owned() }
}
