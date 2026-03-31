// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::{
    ruma::{
        UserId,
        UInt,
        events::{
            AnySyncMessageLikeEvent, AnySyncStateEvent, AnySyncTimelineEvent,
            room::{
                MediaSource,
                message::{
                    AudioMessageEventContent, FileMessageEventContent, ImageMessageEventContent,
                    MessageType, Relation, SyncRoomMessageEvent, VideoMessageEventContent,
                },
            },
        },
    },
};
use matrix_sdk_ui::timeline::{
    AnyOtherFullStateEventContent, InReplyToDetails, MemberProfileChange, MembershipChange,
    MsgLikeContent, MsgLikeKind, OtherState, ReactionsByKeyBySender, RoomMembershipChange,
    TimelineDetails, TimelineEventItemId, TimelineItemContent,
};

use super::MatrixReactionSummary;

pub struct MatrixEventSummary {
    pub kind: String,
    pub matrix_event_type: String,
    pub body: String,
    pub formatted_body: String,
    pub thread_root_id: String,
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
    pub media_is_encrypted: bool,
    pub thumbnail_is_encrypted: bool,
    pub source: Option<MediaSource>,
    pub thumbnail_source: Option<MediaSource>,
}

struct ReplyPreviewSummary {
    event_id: String,
    sender_id: String,
    sender_display_name: String,
    item_kind: String,
    matrix_event_type: String,
    body: String,
    formatted_body: String,
    media: Option<MatrixEventMediaSummary>,
}

pub fn summarize_timeline_content(
    content: &TimelineItemContent,
    own_user_id: Option<&UserId>,
) -> MatrixEventSummary {
    match content {
        TimelineItemContent::MsgLike(content) => summarize_msg_like_content(content, own_user_id),
        TimelineItemContent::MembershipChange(change) => summarize_membership_change(change),
        TimelineItemContent::ProfileChange(change) => summarize_profile_change(change),
        TimelineItemContent::OtherState(state) => summarize_other_state(state),
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
        TimelineItemContent::RtcNotification => summary(
            "rtc_notification",
            "m.rtc.notification",
            "[RTC notification]",
        ),
    }
}

fn summarize_msg_like_content(
    content: &MsgLikeContent,
    own_user_id: Option<&UserId>,
) -> MatrixEventSummary {
    let mut summary = summarize_msg_like_kind(&content.kind);

    summary.thread_root_id = content
        .thread_root
        .as_ref()
        .map(ToString::to_string)
        .unwrap_or_default();

    if let Some(reply_preview) = summarize_reply_preview(content.in_reply_to.as_ref()) {
        summary.reply_event_id = reply_preview.event_id;
        summary.reply_sender_id = reply_preview.sender_id;
        summary.reply_sender_display_name = reply_preview.sender_display_name;
        summary.reply_item_kind = reply_preview.item_kind;
        summary.reply_matrix_event_type = reply_preview.matrix_event_type;
        summary.reply_body = reply_preview.body;
        summary.reply_formatted_body = reply_preview.formatted_body;
        summary.reply_media = reply_preview.media;
    }

    summary.reactions = summarize_reaction_items(&content.reactions, own_user_id);
    summary.reactions_summary = summarize_reactions(&summary.reactions);
    summary.is_edited = matches!(&content.kind, MsgLikeKind::Message(message) if message.is_edited());

    summary
}

fn summarize_msg_like_kind(kind: &MsgLikeKind) -> MatrixEventSummary {
    match kind {
        MsgLikeKind::Message(message) => summary_from_message_type(message.msgtype()),
        MsgLikeKind::Sticker(sticker) => summarize_sticker(sticker.content()),
        MsgLikeKind::Poll(_) => summary("poll", "", "[Poll]"),
        MsgLikeKind::Redacted => summary("redacted", "m.room.message", "Deleted message"),
        MsgLikeKind::UnableToDecrypt(_) => {
            summary("unable_to_decrypt", "m.room.encrypted", "[Unable to decrypt message]")
        }
        MsgLikeKind::Other(other) => {
            let event_type = other.event_type().to_string();
            summary("other_message", &event_type, "[Unsupported message event]")
        }
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
            summary("rtc_notification", "m.rtc.notification", "[RTC notification]")
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
        SyncRoomMessageEvent::Redacted(_) => {
            summary("redacted", "m.room.message", "Deleted message")
        }
    }
}

fn summarize_membership_change(change: &RoomMembershipChange) -> MatrixEventSummary {
    let user = human_name(change.display_name().as_deref(), change.user_id().as_str());

    match change.change() {
        Some(MembershipChange::Joined) => {
            summary("membership_change", "m.room.member", &format!("{user} joined the room"))
        }
        Some(MembershipChange::Left) => {
            summary("membership_change", "m.room.member", &format!("{user} left the room"))
        }
        Some(MembershipChange::Banned) => {
            summary("membership_change", "m.room.member", &format!("{user} was banned"))
        }
        Some(MembershipChange::Unbanned) => {
            summary("membership_change", "m.room.member", &format!("{user} was unbanned"))
        }
        Some(MembershipChange::Kicked) => {
            summary("membership_change", "m.room.member", &format!("{user} was kicked"))
        }
        Some(MembershipChange::Invited) => {
            summary("membership_change", "m.room.member", &format!("{user} was invited"))
        }
        Some(MembershipChange::KickedAndBanned) => {
            summary(
                "membership_change",
                "m.room.member",
                &format!("{user} was kicked and banned"),
            )
        }
        Some(MembershipChange::InvitationAccepted) => {
            summary(
                "membership_change",
                "m.room.member",
                &format!("{user} accepted the invite"),
            )
        }
        Some(MembershipChange::InvitationRejected) => {
            summary(
                "membership_change",
                "m.room.member",
                &format!("{user} rejected the invite"),
            )
        }
        Some(MembershipChange::InvitationRevoked) => {
            summary(
                "membership_change",
                "m.room.member",
                &format!("{user}'s invite was revoked"),
            )
        }
        Some(MembershipChange::Knocked) => {
            summary(
                "membership_change",
                "m.room.member",
                &format!("{user} requested to join"),
            )
        }
        Some(MembershipChange::KnockAccepted) => {
            summary(
                "membership_change",
                "m.room.member",
                &format!("{user}'s knock was accepted"),
            )
        }
        Some(MembershipChange::KnockRetracted) => {
            summary(
                "membership_change",
                "m.room.member",
                &format!("{user} withdrew the join request"),
            )
        }
        Some(MembershipChange::KnockDenied) => {
            summary(
                "membership_change",
                "m.room.member",
                &format!("{user}'s join request was denied"),
            )
        }
        Some(MembershipChange::None)
        | Some(MembershipChange::Error)
        | Some(MembershipChange::NotImplemented)
        | None => summary(
            "membership_change",
            "m.room.member",
            &format!("Membership updated for {user}"),
        ),
    }
}

fn summarize_profile_change(change: &MemberProfileChange) -> MatrixEventSummary {
    let user = human_name(None, change.user_id().as_str());

    if let Some(displayname_change) = change.displayname_change() {
        if let Some(new_name) = displayname_change.new.as_deref().filter(|name| !name.is_empty()) {
            return summary(
                "profile_change",
                "m.room.member",
                &format!("{user} is now known as {new_name}"),
            );
        }
    }

    if change.avatar_url_change().is_some() {
        return summary(
            "profile_change",
            "m.room.member",
            &format!("{user} changed their avatar"),
        );
    }

    summary(
        "profile_change",
        "m.room.member",
        &format!("{user} updated their profile"),
    )
}

fn summarize_other_state(state: &OtherState) -> MatrixEventSummary {
    let event_type = state.content().event_type().to_string();

    match state.content() {
        AnyOtherFullStateEventContent::RoomName(content) => match content {
            matrix_sdk::ruma::events::FullStateEventContent::Original { content, .. }
                if !content.name.is_empty() =>
            {
                summary(
                    "other_state",
                    &event_type,
                    &format!("Room name changed to {}", content.name),
                )
            }
            _ => summary("other_state", &event_type, "Room name changed"),
        },
        AnyOtherFullStateEventContent::RoomTopic(content) => match content {
            matrix_sdk::ruma::events::FullStateEventContent::Original { content, .. }
                if !content.topic.is_empty() =>
            {
                summary(
                    "other_state",
                    &event_type,
                    &format!("Room topic changed to {}", content.topic),
                )
            }
            _ => summary("other_state", &event_type, "Room topic changed"),
        },
        AnyOtherFullStateEventContent::RoomAvatar(_) => {
            summary("other_state", &event_type, "Room avatar changed")
        }
        AnyOtherFullStateEventContent::RoomEncryption(_) => {
            summary("other_state", &event_type, "Enabled end-to-end encryption")
        }
        AnyOtherFullStateEventContent::RoomPinnedEvents(_) => {
            summary("other_state", &event_type, "Pinned messages changed")
        }
        AnyOtherFullStateEventContent::RoomPowerLevels(_) => {
            summary("other_state", &event_type, "Room permissions changed")
        }
        AnyOtherFullStateEventContent::RoomJoinRules(_) => {
            summary("other_state", &event_type, "Room access rules changed")
        }
        AnyOtherFullStateEventContent::RoomHistoryVisibility(_) => {
            summary("other_state", &event_type, "Room history visibility changed")
        }
        AnyOtherFullStateEventContent::RoomGuestAccess(_) => {
            summary("other_state", &event_type, "Room guest access changed")
        }
        AnyOtherFullStateEventContent::RoomCanonicalAlias(_) => {
            summary("other_state", &event_type, "Room alias changed")
        }
        AnyOtherFullStateEventContent::RoomTombstone(_) => {
            summary("other_state", &event_type, "Room was replaced")
        }
        other => summary(
            "other_state",
            &event_type,
            &format!("State event: {}", other.event_type()),
        ),
    }
}

fn formatted_html(message_type: &MessageType) -> &str {
    match message_type {
        MessageType::Text(content) => content
            .formatted
            .as_ref()
            .map(|f| f.body.as_str())
            .unwrap_or(""),
        MessageType::Notice(content) => content
            .formatted
            .as_ref()
            .map(|f| f.body.as_str())
            .unwrap_or(""),
        MessageType::Emote(content) => content
            .formatted
            .as_ref()
            .map(|f| f.body.as_str())
            .unwrap_or(""),
        _ => "",
    }
}

fn summary_from_message_type(message_type: &MessageType) -> MatrixEventSummary {
    match message_type {
        MessageType::Text(_) | MessageType::Notice(_) | MessageType::Emote(_) => {
            let kind = match message_type {
                MessageType::Text(_) => "message",
                MessageType::Notice(_) => "notice",
                MessageType::Emote(_) => "emote",
                _ => unreachable!(),
            };
            let mut s = summary(kind, "m.room.message", message_type.body());
            s.formatted_body = formatted_html(message_type).to_owned();
            s.special_effect_names =
                detect_special_effect_names(message_type.body(), Some(message_type.msgtype()));
            s
        }
        MessageType::_Custom(_) => {
            let mut s = summary("unknown_message", "m.room.message", message_type.body());
            s.special_effect_names =
                detect_special_effect_names(message_type.body(), Some(message_type.msgtype()));
            s
        }
        MessageType::Image(content) => {
            summary_with_media(
                "image",
                "m.room.message",
                caption_or_filename(content),
                media_for_image(content),
            )
        }
        MessageType::Video(content) => {
            summary_with_media(
                "video",
                "m.room.message",
                caption_or_filename(content),
                media_for_video(content),
            )
        }
        MessageType::Audio(content) => {
            summary_with_media(
                "audio",
                "m.room.message",
                caption_or_filename(content),
                media_for_audio(content),
            )
        }
        MessageType::File(content) => {
            summary_with_media(
                "file",
                "m.room.message",
                caption_or_filename(content),
                media_for_file(content),
            )
        }
        MessageType::Location(_) => summary("location", "m.room.message", message_type.body()),
        _ => summary("unknown_message", "m.room.message", message_type.body()),
    }
}

fn append_unique_effect(target: &mut Vec<String>, effect_name: &str) {
    if !target.iter().any(|existing| existing == effect_name) {
        target.push(effect_name.to_owned());
    }
}

fn body_contains_any(body: &str, triggers: &[&str]) -> bool {
    triggers.iter().any(|trigger| body.contains(trigger))
}

fn detect_special_effect_names(body: &str, msgtype: Option<&str>) -> Vec<String> {
    const CONFETTI_TRIGGERS: &[&str] = &["🎉", "🎊"];
    const SUNLIGHT_TRIGGERS: &[&str] = &["☀", "🌞"];
    const LOVE_TRIGGERS: &[&str] = &[
        "❤", "🫶", "💕", "💓", "💘", "💖", "💗", "💞", "💝", "💟", "❣", "😻", "😘", "🥰",
    ];
    const LOVE_FACE_TRIGGERS: &[&str] = &["😍"];
    const RAINFALL_TRIGGERS: &[&str] = &["🌧", "🌦", "☔"];
    const LIGHTNING_TRIGGERS: &[&str] = &["⚡"];
    const STORM_TRIGGERS: &[&str] = &["⛈"];
    const KOMAI_LOGO_TRIGGERS: &[&str] = &["🦁", "⛩️"];

    let mut effects = Vec::new();

    if body_contains_any(body, CONFETTI_TRIGGERS) {
        append_unique_effect(&mut effects, "confetti");
    }
    if body_contains_any(body, SUNLIGHT_TRIGGERS) {
        append_unique_effect(&mut effects, "sunlight");
    }
    if body_contains_any(body, LOVE_TRIGGERS) || body_contains_any(body, LOVE_FACE_TRIGGERS) {
        append_unique_effect(&mut effects, "love");
    }
    if body_contains_any(body, RAINFALL_TRIGGERS) || body_contains_any(body, STORM_TRIGGERS) {
        append_unique_effect(&mut effects, "rainfall");
    }
    if body_contains_any(body, LIGHTNING_TRIGGERS) || body_contains_any(body, STORM_TRIGGERS) {
        append_unique_effect(&mut effects, "lightning");
    }
    if body_contains_any(body, KOMAI_LOGO_TRIGGERS) {
        append_unique_effect(&mut effects, "komaiLogo");
    }

    match msgtype.unwrap_or_default() {
        "nic.custom.confetti" | "nic.custom.fireworks" => {
            append_unique_effect(&mut effects, "confetti");
        }
        "io.element.effect.rainfall" => {
            append_unique_effect(&mut effects, "rainfall");
        }
        "io.element.effect.hearts" => {
            append_unique_effect(&mut effects, "love");
        }
        _ => {}
    }

    effects
}

fn human_name(display_name: Option<&str>, user_id: &str) -> String {
    display_name
        .map(str::trim)
        .filter(|name| !name.is_empty())
        .unwrap_or(user_id)
        .to_owned()
}

fn summary(kind: &str, matrix_event_type: &str, body: &str) -> MatrixEventSummary {
    MatrixEventSummary {
        kind: kind.to_owned(),
        matrix_event_type: matrix_event_type.to_owned(),
        body: body.to_owned(),
        formatted_body: String::new(),
        thread_root_id: String::new(),
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
    }
}

fn summary_with_media(
    kind: &str,
    matrix_event_type: &str,
    body: &str,
    media: MatrixEventMediaSummary,
) -> MatrixEventSummary {
    MatrixEventSummary {
        kind: kind.to_owned(),
        matrix_event_type: matrix_event_type.to_owned(),
        body: body.to_owned(),
        formatted_body: String::new(),
        thread_root_id: String::new(),
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
        media: Some(media),
    }
}

fn summarize_reply_preview(
    details: Option<&InReplyToDetails>,
) -> Option<ReplyPreviewSummary> {
    let details = details?;
    let reply_event_id = details.event_id.to_string();

    match &details.event {
        TimelineDetails::Ready(event) => {
            let resolved_reply_event_id = match &event.identifier {
                TimelineEventItemId::EventId(event_id) => event_id.to_string(),
                TimelineEventItemId::TransactionId(transaction_id) => transaction_id.to_string(),
            };
            let sender_id = event.sender.to_string();
            let sender_display_name = match &event.sender_profile {
                TimelineDetails::Ready(profile) => {
                    human_name(profile.display_name.as_deref(), event.sender.as_str())
                }
                _ => human_name(None, event.sender.as_str()),
            };
            let reply_summary = summarize_embedded_content(&event.content);
            Some(ReplyPreviewSummary {
                event_id: if resolved_reply_event_id.is_empty() {
                    reply_event_id
                } else {
                    resolved_reply_event_id
                },
                sender_id,
                sender_display_name,
                item_kind: reply_summary.kind,
                matrix_event_type: reply_summary.matrix_event_type,
                body: reply_summary.body,
                formatted_body: reply_summary.formatted_body,
                media: reply_summary.media,
            })
        }
        TimelineDetails::Unavailable | TimelineDetails::Pending | TimelineDetails::Error(_) => {
            Some(ReplyPreviewSummary {
                event_id: reply_event_id,
                sender_id: String::new(),
                sender_display_name: String::new(),
                item_kind: String::new(),
                matrix_event_type: String::new(),
                body: "[Original message unavailable]".to_owned(),
                formatted_body: String::new(),
                media: None,
            })
        }
    }
}

fn summarize_embedded_content(content: &TimelineItemContent) -> MatrixEventSummary {
    summarize_timeline_content(content, None)
}

fn summarize_reaction_items(
    reactions: &ReactionsByKeyBySender,
    own_user_id: Option<&UserId>,
) -> Vec<MatrixReactionSummary> {
    reactions
        .iter()
        .take(6)
        .map(|(key, senders)| {
            let users = senders
                .keys()
                .map(|sender_id| sender_id.to_string())
                .collect::<Vec<_>>()
                .join(", ");
            let self_reacted_event = own_user_id
                .and_then(|user_id| senders.get(user_id))
                .map(|info| match &info.status {
                    matrix_sdk_ui::timeline::ReactionStatus::RemoteToRemote(event_id) => {
                        event_id.to_string()
                    }
                    matrix_sdk_ui::timeline::ReactionStatus::LocalToLocal(_)
                    | matrix_sdk_ui::timeline::ReactionStatus::LocalToRemote(_) => {
                        "__local__".to_owned()
                    }
                })
                .unwrap_or_default();

            MatrixReactionSummary {
                key: key.clone(),
                users,
                self_reacted_event,
                count: senders.len() as u64,
            }
        })
        .collect()
}

fn summarize_reactions(reactions: &[MatrixReactionSummary]) -> String {
    reactions
        .iter()
        .map(|reaction| format!("{} {}", reaction.key, reaction.count))
        .collect::<Vec<_>>()
        .join("  ")
}

fn summarize_sticker(
    content: &matrix_sdk::ruma::events::sticker::StickerEventContent,
) -> MatrixEventSummary {
    let media_source = sticker_media_source_to_media_source(&content.source);
    let media_url = media_source
        .as_ref()
        .map(media_source_url)
        .unwrap_or_default();
    let thumbnail_url = media_source
        .as_ref()
        .map(|primary_source| {
            thumbnail_url_or_primary(content.info.thumbnail_source.as_ref(), primary_source)
        })
        .unwrap_or_default();
    let media_is_encrypted = media_source
        .as_ref()
        .map(media_source_is_encrypted)
        .unwrap_or(false);
    let thumbnail_is_encrypted = media_source
        .as_ref()
        .map(|primary_source| {
            thumbnail_is_encrypted_or_primary(content.info.thumbnail_source.as_ref(), primary_source)
        })
        .unwrap_or(false);

    summary_with_media(
        "sticker",
        "m.sticker",
        content.body.as_str(),
        MatrixEventMediaSummary {
            media_url,
            thumbnail_url,
            file_name: content.body.clone(),
            mime_type: content.info.mimetype.clone().unwrap_or_default(),
            media_width: opt_uint_to_u64(content.info.width),
            media_height: opt_uint_to_u64(content.info.height),
            media_duration_ms: 0,
            media_size_bytes: opt_uint_to_u64(content.info.size),
            media_is_encrypted,
            thumbnail_is_encrypted,
            source: media_source,
            thumbnail_source: content.info.thumbnail_source.clone(),
        },
    )
}

fn media_for_image(content: &ImageMessageEventContent) -> MatrixEventMediaSummary {
    let info = content.info.as_deref();
    let thumbnail_source = info.and_then(|info| info.thumbnail_source.clone());

    MatrixEventMediaSummary {
        media_url: media_source_url(&content.source),
        thumbnail_url: thumbnail_url_or_primary(thumbnail_source.as_ref(), &content.source),
        file_name: content.filename().to_owned(),
        mime_type: info.and_then(|info| info.mimetype.clone()).unwrap_or_default(),
        media_width: info.and_then(|info| info.width).map_or(0, uint_to_u64),
        media_height: info.and_then(|info| info.height).map_or(0, uint_to_u64),
        media_duration_ms: 0,
        media_size_bytes: info.and_then(|info| info.size).map_or(0, uint_to_u64),
        media_is_encrypted: media_source_is_encrypted(&content.source),
        thumbnail_is_encrypted: thumbnail_is_encrypted_or_primary(
            thumbnail_source.as_ref(),
            &content.source,
        ),
        source: Some(content.source.clone()),
        thumbnail_source,
    }
}

fn media_for_video(content: &VideoMessageEventContent) -> MatrixEventMediaSummary {
    let info = content.info.as_deref();
    let thumbnail_source = info.and_then(|info| info.thumbnail_source.clone());

    MatrixEventMediaSummary {
        media_url: media_source_url(&content.source),
        thumbnail_url: thumbnail_source
            .as_ref()
            .map(media_source_url)
            .unwrap_or_default(),
        file_name: content.filename().to_owned(),
        mime_type: info.and_then(|info| info.mimetype.clone()).unwrap_or_default(),
        media_width: info.and_then(|info| info.width).map_or(0, uint_to_u64),
        media_height: info.and_then(|info| info.height).map_or(0, uint_to_u64),
        media_duration_ms: info
            .and_then(|info| info.duration)
            .map_or(0, duration_to_millis_u64),
        media_size_bytes: info.and_then(|info| info.size).map_or(0, uint_to_u64),
        media_is_encrypted: media_source_is_encrypted(&content.source),
        thumbnail_is_encrypted: thumbnail_source
            .as_ref()
            .map(media_source_is_encrypted)
            .unwrap_or(false),
        source: Some(content.source.clone()),
        thumbnail_source,
    }
}

fn media_for_audio(content: &AudioMessageEventContent) -> MatrixEventMediaSummary {
    let info = content.info.as_deref();

    MatrixEventMediaSummary {
        media_url: media_source_url(&content.source),
        thumbnail_url: String::new(),
        file_name: content.filename().to_owned(),
        mime_type: info.and_then(|info| info.mimetype.clone()).unwrap_or_default(),
        media_width: 0,
        media_height: 0,
        media_duration_ms: info
            .and_then(|info| info.duration)
            .map_or(0, duration_to_millis_u64),
        media_size_bytes: info.and_then(|info| info.size).map_or(0, uint_to_u64),
        media_is_encrypted: media_source_is_encrypted(&content.source),
        thumbnail_is_encrypted: false,
        source: Some(content.source.clone()),
        thumbnail_source: None,
    }
}

fn media_for_file(content: &FileMessageEventContent) -> MatrixEventMediaSummary {
    let info = content.info.as_deref();
    let thumbnail_source = info.and_then(|info| info.thumbnail_source.clone());

    MatrixEventMediaSummary {
        media_url: media_source_url(&content.source),
        thumbnail_url: thumbnail_source
            .as_ref()
            .map(media_source_url)
            .unwrap_or_default(),
        file_name: content.filename().to_owned(),
        mime_type: info.and_then(|info| info.mimetype.clone()).unwrap_or_default(),
        media_width: 0,
        media_height: 0,
        media_duration_ms: 0,
        media_size_bytes: info.and_then(|info| info.size).map_or(0, uint_to_u64),
        media_is_encrypted: media_source_is_encrypted(&content.source),
        thumbnail_is_encrypted: thumbnail_source
            .as_ref()
            .map(media_source_is_encrypted)
            .unwrap_or(false),
        source: Some(content.source.clone()),
        thumbnail_source,
    }
}

fn caption_or_filename<T>(content: &T) -> &str
where
    T: MediaCaption,
{
    content.caption().unwrap_or_else(|| content.filename())
}

trait MediaCaption {
    fn filename(&self) -> &str;
    fn caption(&self) -> Option<&str>;
}

impl MediaCaption for ImageMessageEventContent {
    fn filename(&self) -> &str { ImageMessageEventContent::filename(self) }
    fn caption(&self) -> Option<&str> { ImageMessageEventContent::caption(self) }
}

impl MediaCaption for VideoMessageEventContent {
    fn filename(&self) -> &str { VideoMessageEventContent::filename(self) }
    fn caption(&self) -> Option<&str> { VideoMessageEventContent::caption(self) }
}

impl MediaCaption for AudioMessageEventContent {
    fn filename(&self) -> &str { AudioMessageEventContent::filename(self) }
    fn caption(&self) -> Option<&str> { AudioMessageEventContent::caption(self) }
}

impl MediaCaption for FileMessageEventContent {
    fn filename(&self) -> &str { FileMessageEventContent::filename(self) }
    fn caption(&self) -> Option<&str> { FileMessageEventContent::caption(self) }
}

fn sticker_media_source_to_media_source(
    source: &matrix_sdk::ruma::events::sticker::StickerMediaSource,
) -> Option<MediaSource> {
    match source {
        matrix_sdk::ruma::events::sticker::StickerMediaSource::Plain(url) => {
            Some(MediaSource::Plain(url.clone()))
        }
        _ => None,
    }
}

fn media_source_url(source: &MediaSource) -> String {
    match source {
        MediaSource::Plain(uri) => uri.to_string(),
        MediaSource::Encrypted(file) => file.url.to_string(),
    }
}

fn media_source_is_encrypted(source: &MediaSource) -> bool {
    matches!(source, MediaSource::Encrypted(_))
}

fn thumbnail_url_or_primary(
    thumbnail_source: Option<&MediaSource>,
    primary_source: &MediaSource,
) -> String {
    thumbnail_source
        .map(media_source_url)
        .unwrap_or_else(|| media_source_url(primary_source))
}

fn thumbnail_is_encrypted_or_primary(
    thumbnail_source: Option<&MediaSource>,
    primary_source: &MediaSource,
) -> bool {
    thumbnail_source
        .map(media_source_is_encrypted)
        .unwrap_or_else(|| media_source_is_encrypted(primary_source))
}

fn opt_uint_to_u64(value: Option<UInt>) -> u64 {
    value.map_or(0, uint_to_u64)
}

fn uint_to_u64(value: UInt) -> u64 {
    u64::from(value)
}

fn duration_to_millis_u64(duration: std::time::Duration) -> u64 {
    u64::try_from(duration.as_millis()).unwrap_or(u64::MAX)
}
