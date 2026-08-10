// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Summarize message-like events: text/media bodies, reply previews,
//! reactions, stickers, and the special-effects keyword detection that
//! powers fireworks/snowfall/etc.

use super::*;
use super::media::{
    caption_or_filename, media_for_audio, media_for_file,
    media_for_image, media_for_video, media_source_is_encrypted, media_source_url,
    opt_uint_to_u64, sticker_media_source_to_media_source,
    thumbnail_is_encrypted_or_primary, thumbnail_url_or_primary,
};
use super::state::human_name;

pub(super) struct ReplyPreviewSummary {
    event_id: String,
    sender_id: String,
    sender_display_name: String,
    item_kind: String,
    matrix_event_type: String,
    body: String,
    formatted_body: String,
    media: Option<MatrixEventMediaSummary>,
}

pub(super) fn summarize_msg_like_content(
    content: &MsgLikeContent,
    own_user_id: Option<&UserId>,
) -> MatrixEventSummary {
    let mut summary = summarize_msg_like_kind(&content.kind);

    summary.thread_root_id = content
        .thread_root
        .as_ref()
        .map(ToString::to_string)
        .unwrap_or_default();
    if let Some(ref ts) = content.thread_summary {
        summary.is_thread_root = true;
        summary.thread_reply_count = ts.num_replies;
    }

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

pub(super) fn summarize_msg_like_kind(kind: &MsgLikeKind) -> MatrixEventSummary {
    match kind {
        MsgLikeKind::Message(message) => summary_from_message_type(message.msgtype()),
        MsgLikeKind::Sticker(sticker) => summarize_sticker(sticker.content()),
        MsgLikeKind::Poll(_) => summary("poll", "", "[Poll]"),
        MsgLikeKind::Redacted => summary("redacted", "m.room.message", "Deleted message"),
        MsgLikeKind::UnableToDecrypt(encrypted_message) => {
            let mut s =
                summary("unable_to_decrypt", "m.room.encrypted", "[Unable to decrypt message]");
            s.utd_cause = utd_cause_tag(encrypted_message).to_owned();
            s
        }
        MsgLikeKind::Other(other) => {
            let event_type = other.event_type().to_string();
            summary("other_message", &event_type, "[Unsupported message event]")
        }
        MsgLikeKind::LiveLocation(_) => {
            summary("live_location", "m.beacon", "[Live location]")
        }
    }
}

/// Map matrix-sdk's `UtdCause` to a stable snake_case tag. Non-Megolm
/// encrypted messages carry no cause information, so report them as `unknown`.
pub(super) fn utd_cause_tag(encrypted_message: &EncryptedMessage) -> &'static str {
    use matrix_sdk_base::crypto::types::events::UtdCause;

    let cause = match encrypted_message {
        EncryptedMessage::MegolmV1AesSha2 { cause, .. } => *cause,
        EncryptedMessage::OlmV1Curve25519AesSha2 { .. } | EncryptedMessage::Unknown => {
            UtdCause::Unknown
        }
    };

    match cause {
        UtdCause::Unknown => "unknown",
        UtdCause::SentBeforeWeJoined => "sent_before_we_joined",
        UtdCause::VerificationViolation => "verification_violation",
        UtdCause::UnsignedDevice => "unsigned_device",
        UtdCause::UnknownDevice => "unknown_device",
        UtdCause::HistoricalMessageAndBackupIsDisabled => {
            "historical_message_and_backup_disabled"
        }
        UtdCause::WithheldForUnverifiedOrInsecureDevice => {
            "withheld_for_unverified_or_insecure_device"
        }
        UtdCause::WithheldBySender => "withheld_by_sender",
        UtdCause::HistoricalMessageAndDeviceIsUnverified => {
            "historical_message_and_device_unverified"
        }
    }
}

/// Map a raw-path decryption failure (`UnableToDecryptReason`) to the same
/// snake_case tags as [`utd_cause_tag`]. Without a `CryptoContextInfo` the
/// historical-message causes cannot be distinguished, so those collapse to
/// `"unknown"`; the mapping otherwise mirrors matrix-sdk's
/// `UtdCause::determine`.
pub fn utd_reason_tag(
    reason: &matrix_sdk::deserialized_responses::UnableToDecryptReason,
    raw: &matrix_sdk::ruma::serde::Raw<AnySyncTimelineEvent>,
) -> &'static str {
    use matrix_sdk::deserialized_responses::{
        UnableToDecryptReason::*, VerificationLevel, WithheldCode,
    };

    match reason {
        MissingMegolmSession { withheld_code: Some(WithheldCode::Unverified) } => {
            "withheld_for_unverified_or_insecure_device"
        }
        MissingMegolmSession { withheld_code: Some(WithheldCode::HistoryNotShared) } => {
            if sent_while_not_joined(raw) { "sent_before_we_joined" } else { "unknown" }
        }
        MissingMegolmSession { withheld_code: Some(_) } => "withheld_by_sender",
        MissingMegolmSession { withheld_code: None } | UnknownMegolmMessageIndex => {
            if sent_while_not_joined(raw) { "sent_before_we_joined" } else { "unknown" }
        }
        SenderIdentityNotTrusted(VerificationLevel::VerificationViolation) => {
            "verification_violation"
        }
        SenderIdentityNotTrusted(VerificationLevel::UnsignedDevice) => "unsigned_device",
        SenderIdentityNotTrusted(VerificationLevel::None(_)) => "unknown_device",
        _ => "unknown",
    }
}

/// Whether the server flagged (via `unsigned.membership`, MSC4115) that we
/// were not in the room when this event was sent.
fn sent_while_not_joined(raw: &matrix_sdk::ruma::serde::Raw<AnySyncTimelineEvent>) -> bool {
    raw.get_field::<serde_json::Value>("unsigned")
        .ok()
        .flatten()
        .and_then(|u| u.get("membership").and_then(|m| m.as_str().map(str::to_owned)))
        .is_some_and(|m| m == "leave")
}

pub(super) fn summarize_room_message_event(message: &SyncRoomMessageEvent) -> MatrixEventSummary {
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

pub(super) fn formatted_html(message_type: &MessageType) -> &str {
    // ruma exposes `formatted: Option<FormattedBody>` on every media content
    // type too (image/video/audio/file), so rich captions on attachments —
    // see Element X's behaviour and Komai's own send path — surface the same
    // way as text-message bodies.
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
        MessageType::Image(content) => content
            .formatted
            .as_ref()
            .map(|f| f.body.as_str())
            .unwrap_or(""),
        MessageType::Video(content) => content
            .formatted
            .as_ref()
            .map(|f| f.body.as_str())
            .unwrap_or(""),
        MessageType::Audio(content) => content
            .formatted
            .as_ref()
            .map(|f| f.body.as_str())
            .unwrap_or(""),
        MessageType::File(content) => content
            .formatted
            .as_ref()
            .map(|f| f.body.as_str())
            .unwrap_or(""),
        _ => "",
    }
}

pub(super) fn summary_from_message_type(message_type: &MessageType) -> MatrixEventSummary {
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
            let mut s = summary_with_media(
                "image",
                "m.room.message",
                caption_or_filename(content),
                media_for_image(content),
            );
            s.formatted_body = formatted_html(message_type).to_owned();
            s
        }
        MessageType::Video(content) => {
            let mut s = summary_with_media(
                "video",
                "m.room.message",
                caption_or_filename(content),
                media_for_video(content),
            );
            s.formatted_body = formatted_html(message_type).to_owned();
            s
        }
        MessageType::Audio(content) => {
            let mut s = summary_with_media(
                "audio",
                "m.room.message",
                caption_or_filename(content),
                media_for_audio(content),
            );
            s.formatted_body = formatted_html(message_type).to_owned();
            s.is_voice_message = content.voice.is_some();
            if let Some(ref audio) = content.audio {
                s.waveform = audio
                    .waveform
                    .iter()
                    .map(|amp| {
                        let v = u64::from(amp.get());
                        (v as f32 / 1024.0).clamp(0.0, 1.0)
                    })
                    .collect();
            }
            s
        }
        MessageType::File(content) => {
            let mut s = summary_with_media(
                "file",
                "m.room.message",
                caption_or_filename(content),
                media_for_file(content),
            );
            s.formatted_body = formatted_html(message_type).to_owned();
            s
        }
        MessageType::Location(_) => summary("location", "m.room.message", message_type.body()),
        _ => summary("unknown_message", "m.room.message", message_type.body()),
    }
}

pub(super) fn append_unique_effect(target: &mut Vec<String>, effect_name: &str) {
    if !target.iter().any(|existing| existing == effect_name) {
        target.push(effect_name.to_owned());
    }
}

pub(super) fn body_contains_any(body: &str, triggers: &[&str]) -> bool {
    triggers.iter().any(|trigger| body.contains(trigger))
}

pub(super) fn detect_special_effect_names(body: &str, msgtype: Option<&str>) -> Vec<String> {
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

pub(super) fn summary_with_media(
    kind: &str,
    matrix_event_type: &str,
    body: &str,
    media: MatrixEventMediaSummary,
) -> MatrixEventSummary {
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
        media: Some(media),
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

pub(super) fn summarize_reply_preview(
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

pub(super) fn summarize_embedded_content(content: &TimelineItemContent) -> MatrixEventSummary {
    summarize_timeline_content(content, None, "")
}

pub(super) fn summarize_reaction_items(
    reactions: &ReactionsByKeyBySender,
    own_user_id: Option<&UserId>,
) -> Vec<MatrixReactionSummary> {
    // The full list of distinct reactions is exposed unconditionally — QML caps
    // the number of pills rendered inline and offers a "+N" pill that opens a
    // details dialog for the remainder.
    const MAX_TOOLTIP_USERS: usize = 10;

    reactions
        .iter()
        .map(|(key, senders)| {
            let user_ids: Vec<String> =
                senders.keys().map(|sender_id| sender_id.to_string()).collect();
            let total_senders = user_ids.len();
            // The tooltip is a single-line-per-user textual list. It stays
            // capped to keep hover tooltips reasonable; the details dialog has
            // the complete list with avatars and is the right place to inspect
            // who reacted.
            let mut tooltip_users: Vec<String> =
                user_ids.iter().take(MAX_TOOLTIP_USERS).cloned().collect();
            if total_senders > MAX_TOOLTIP_USERS {
                tooltip_users.push(format!(
                    "… and {} more",
                    total_senders - MAX_TOOLTIP_USERS
                ));
            }
            let users = tooltip_users.join("\n");
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
                user_ids,
                self_reacted_event,
                count: senders.len() as u64,
            }
        })
        .collect()
}

pub(super) fn summarize_reactions(reactions: &[MatrixReactionSummary]) -> String {
    reactions
        .iter()
        .map(|reaction| format!("{} {}", reaction.key, reaction.count))
        .collect::<Vec<_>>()
        .join("  ")
}

pub(super) fn summarize_sticker(
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
            blurhash: content.info.blurhash.clone().unwrap_or_default(),
            media_is_encrypted,
            thumbnail_is_encrypted,
            source: media_source,
            thumbnail_source: content.info.thumbnail_source.clone(),
        },
    )
}
