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
    pub body: String,
    pub reply_event_id: String,
    pub reply_sender_id: String,
    pub reply_sender_display_name: String,
    pub reply_body: String,
    pub reactions: Vec<MatrixReactionSummary>,
    pub reactions_summary: String,
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

pub fn summarize_timeline_content(
    content: &TimelineItemContent,
    own_user_id: Option<&UserId>,
) -> MatrixEventSummary {
    match content {
        TimelineItemContent::MsgLike(content) => summarize_msg_like_content(content, own_user_id),
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

fn summarize_msg_like_content(
    content: &MsgLikeContent,
    own_user_id: Option<&UserId>,
) -> MatrixEventSummary {
    let mut summary = summarize_msg_like_kind(&content.kind);

    if let Some((reply_event_id, reply_sender_id, reply_sender_display_name, reply_body)) =
        summarize_reply_preview(content.in_reply_to.as_ref())
    {
        summary.reply_event_id = reply_event_id;
        summary.reply_sender_id = reply_sender_id;
        summary.reply_sender_display_name = reply_sender_display_name;
        summary.reply_body = reply_body;
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
        MsgLikeKind::Poll(_) => summary("poll", "[Poll]"),
        MsgLikeKind::Redacted => summary("redacted", "[Redacted message]"),
        MsgLikeKind::UnableToDecrypt(_) => summary("unable_to_decrypt", "[Unable to decrypt message]"),
        MsgLikeKind::Other(_) => summary("other_message", "[Unsupported message event]"),
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
        MessageType::Image(content) => {
            summary_with_media("image", caption_or_filename(content), media_for_image(content))
        }
        MessageType::Video(content) => {
            summary_with_media("video", caption_or_filename(content), media_for_video(content))
        }
        MessageType::Audio(content) => {
            summary_with_media("audio", caption_or_filename(content), media_for_audio(content))
        }
        MessageType::File(content) => {
            summary_with_media("file", caption_or_filename(content), media_for_file(content))
        }
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
    MatrixEventSummary {
        kind: kind.to_owned(),
        body: body.to_owned(),
        reply_event_id: String::new(),
        reply_sender_id: String::new(),
        reply_sender_display_name: String::new(),
        reply_body: String::new(),
        reactions: Vec::new(),
        reactions_summary: String::new(),
        is_edited: false,
        media: None,
    }
}

fn summary_with_media(
    kind: &str,
    body: &str,
    media: MatrixEventMediaSummary,
) -> MatrixEventSummary {
    MatrixEventSummary {
        kind: kind.to_owned(),
        body: body.to_owned(),
        reply_event_id: String::new(),
        reply_sender_id: String::new(),
        reply_sender_display_name: String::new(),
        reply_body: String::new(),
        reactions: Vec::new(),
        reactions_summary: String::new(),
        is_edited: false,
        media: Some(media),
    }
}

fn summarize_reply_preview(
    details: Option<&InReplyToDetails>,
) -> Option<(String, String, String, String)> {
    let details = details?;

    match &details.event {
        TimelineDetails::Ready(event) => {
            let reply_event_id = match &event.identifier {
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
            Some((
                reply_event_id,
                sender_id,
                sender_display_name,
                reply_summary.body,
            ))
        }
        TimelineDetails::Unavailable | TimelineDetails::Pending | TimelineDetails::Error(_) => {
            Some((
                String::new(),
                String::new(),
                String::new(),
                "[Original message unavailable]".to_owned(),
            ))
        }
    }
}

fn summarize_embedded_content(content: &TimelineItemContent) -> MatrixEventSummary {
    match content {
        TimelineItemContent::MsgLike(content) => summarize_msg_like_kind(&content.kind),
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
        TimelineItemContent::RtcNotification => summary("rtc_notification", "[RTC notification]"),
    }
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
