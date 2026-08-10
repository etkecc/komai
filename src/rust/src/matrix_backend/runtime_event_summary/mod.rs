// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::{
    ruma::{
        UserId,
        UInt,
        events::{
            AnySyncMessageLikeEvent, AnySyncStateEvent, AnySyncTimelineEvent,
            StateEventContentChange,
            room::{
                MediaSource,
                guest_access::GuestAccess,
                history_visibility::HistoryVisibility,
                join_rules::JoinRule,
                message::{
                    AudioMessageEventContent, FileMessageEventContent, ImageMessageEventContent,
                    MessageType, Relation, SyncRoomMessageEvent, VideoMessageEventContent,
                },
            },
        },
    },
};
use matrix_sdk_ui::timeline::{
    AnyOtherStateEventContentChange, EncryptedMessage, InReplyToDetails, MemberProfileChange,
    MembershipChange, MsgLikeContent, MsgLikeKind, OtherState, ReactionsByKeyBySender,
    RoomMembershipChange, TimelineDetails, TimelineEventItemId, TimelineItemContent,
};

use super::MatrixReactionSummary;

pub struct MatrixEventSummary {
    pub kind: String,
    pub membership_change_kind: String,
    pub matrix_event_type: String,
    pub body: String,
    pub formatted_body: String,
    pub thread_root_id: String,
    pub is_thread_root: bool,
    pub thread_reply_count: u32,
    pub reply_event_id: String,
    pub reply_sender_id: String,
    pub reply_sender_display_name: String,
    pub reply_item_kind: String,
    pub reply_matrix_event_type: String,
    pub reply_body: String,
    pub reply_formatted_body: String,
    pub reply_media: Option<MatrixEventMediaSummary>,
    pub reactions: Vec<MatrixReactionSummary>,
    pub reactions_summary: String,
    pub special_effect_names: Vec<String>,
    pub is_edited: bool,
    pub media: Option<MatrixEventMediaSummary>,
    pub is_voice_message: bool,
    pub waveform: Vec<f32>,
    /// The affected user's display name (for membership/profile events).
    pub state_event_target_user: String,
    /// The affected user's MXID (for membership/profile events).
    pub state_event_target_user_id: String,
    /// Dynamic value: new room name, topic, join rule key, etc.
    pub state_event_detail: String,
    /// Reason for kicks/bans.
    pub state_event_reason: String,
    /// Whether the sender is distinct from the target user.
    pub state_event_has_sender: bool,
    /// Cause tag for `unable_to_decrypt` items. Empty for every other kind.
    /// Values are snake_case names from matrix-sdk's `UtdCause`, e.g.
    /// `"sent_before_we_joined"`, `"withheld_by_sender"`.
    pub utd_cause: String,
    /// Power level user changes for enriched m.room.power_levels messages.
    pub power_level_changes: Vec<super::event_detail::PowerLevelChange>,
    /// Server ACL changes for enriched m.room.server_acl messages.
    pub server_acl_changes: Option<super::event_detail::ServerAclChange>,
    /// For `m.room.tombstone` state events, the room id the tombstone
    /// points at (`content.replacement_room`).  Empty for every other
    /// event kind.
    pub tombstone_replacement_room_id: String,
}

#[derive(Clone, Debug)]
pub struct MatrixEventMediaSummary {
    pub media_url: String,
    pub thumbnail_url: String,
    pub file_name: String,
    pub mime_type: String,
    pub media_width: u64,
    pub media_height: u64,
    pub media_duration_ms: u64,
    pub media_size_bytes: u64,
    pub blurhash: String,
    pub media_is_encrypted: bool,
    pub thumbnail_is_encrypted: bool,
    pub source: Option<MediaSource>,
    pub thumbnail_source: Option<MediaSource>,
}

mod media;
mod messages;
mod state;

use messages::{summarize_msg_like_content, summarize_room_message_event};
use state::{summarize_membership_change, summarize_other_state, summarize_profile_change};
pub use messages::utd_reason_tag;
pub use state::summarize_sync_state_event;

/// Summarize a timeline event into structured data for the C++ UI layer.
///
/// State events and event-type labels are translated to user-visible text
/// by C++ `StateEventText` (src/timeline/StateEventText.cpp).
/// When adding new item_kind values or state event types here, add
/// corresponding tr() calls there.
pub fn summarize_timeline_content(
    content: &TimelineItemContent,
    own_user_id: Option<&UserId>,
    sender_display_name: &str,
) -> MatrixEventSummary {
    match content {
        TimelineItemContent::MsgLike(content) => summarize_msg_like_content(content, own_user_id),
        TimelineItemContent::MembershipChange(change) => {
            summarize_membership_change(change, sender_display_name)
        }
        TimelineItemContent::ProfileChange(change) => summarize_profile_change(change),
        TimelineItemContent::OtherState(state) => summarize_other_state(state, sender_display_name),
        TimelineItemContent::FailedToParseMessageLike { event_type, .. } => {
            let event_type = event_type.to_string();
            summary(
                "failed_to_parse_message_like",
                &event_type,
                "[Unreadable message event]",
            )
        }
        TimelineItemContent::FailedToParseState { event_type, .. } => {
            let event_type = event_type.to_string();
            summary("failed_to_parse_state", &event_type, "[Unreadable state event]")
        }
        TimelineItemContent::CallInvite => summary("call_invite", "m.call.invite", "[Call invite]"),
        TimelineItemContent::RtcNotification { .. } => summary(
            "rtc_notification",
            "m.rtc.notification",
            "Started a call",
        ),
    }
}

pub fn summarize_sync_timeline_event(event: &AnySyncTimelineEvent) -> Option<MatrixEventSummary> {
    let summary = match event {
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomMessage(message)) => {
            summarize_room_message_event(message)
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::Sticker(_)) => {
            summary("sticker", "m.sticker", "[Sticker]")
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::UnstablePollStart(_)) => {
            summary("poll", "", "[Poll]")
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::CallInvite(_)) => {
            summary("call_invite", "m.call.invite", "[Call invite]")
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RtcNotification(_)) => {
            summary("rtc_notification", "m.rtc.notification", "Started a call")
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::Reaction(_)) => {
            summary("reaction", "m.reaction", "Reactions updated")
        }
        AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::RoomRedaction(_)) => {
            summary("redacted", "m.room.redaction", "Deleted message")
        }
        AnySyncTimelineEvent::State(AnySyncStateEvent::RoomMember(_)) => {
            summary("membership_change", "m.room.member", "[Membership change]")
        }
        AnySyncTimelineEvent::State(state) => {
            let event_type = state.event_type().to_string();
            summary("other_state", &event_type, &format!("State event: {}", event_type))
        }
        AnySyncTimelineEvent::MessageLike(message) => {
            let event_type = message.event_type().to_string();
            summary("other_message", &event_type, &format!("[{}]", event_type))
        }
    };

    Some(summary)
}

pub(super) fn summary(kind: &str, matrix_event_type: &str, body: &str) -> MatrixEventSummary {
    MatrixEventSummary {
        kind: kind.to_owned(),
        membership_change_kind: String::new(),
        matrix_event_type: matrix_event_type.to_owned(),
        body: body.to_owned(),
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
}

#[cfg(test)]
mod tests;
