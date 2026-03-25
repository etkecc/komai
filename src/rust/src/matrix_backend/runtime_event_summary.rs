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
use matrix_sdk_ui::timeline::{MsgLikeKind, TimelineItemContent};

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
        TimelineItemContent::MembershipChange(_) => {
            summary("membership_change", "[Membership change]")
        }
        TimelineItemContent::ProfileChange(_) => summary("profile_change", "[Profile change]"),
        TimelineItemContent::OtherState(_) => summary("other_state", "[State event]"),
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

fn summary(kind: &str, body: &str) -> MatrixEventSummary {
    MatrixEventSummary { kind: kind.to_owned(), body: body.to_owned() }
}
