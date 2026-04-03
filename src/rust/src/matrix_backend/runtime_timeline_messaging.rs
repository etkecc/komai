// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Message sending, editing, reactions, redactions, read receipts, and reporting.

use super::*;

use matrix_sdk::{
    attachment::{
        AttachmentConfig, AttachmentInfo, BaseAudioInfo, BaseFileInfo, BaseImageInfo,
        BaseVideoInfo,
    },
    room::{Receipts, ReportedContentScore},
    room::edit::EditedContent,
    room::reply::{EnforceThread, Reply},
    ruma::{
        EventId, UInt,
        html::{HtmlSanitizerMode, RemoveReplyFallback},
        events::EventContentFromType,
        events::room::message::{
            MessageType, RoomMessageEventContentWithoutRelation, TextMessageEventContent,
        },
    },
};
use image::GenericImageView;
use mime::Mime;
use serde_json::{Map as JsonMap, Value as JsonValue};
use std::{fs, path::Path};

// ---------------------------------------------------------------------------
// Message formatting helpers
// ---------------------------------------------------------------------------

fn normalized_message_kind(message_kind: &str) -> &str {
    match message_kind.trim() {
        "message" | "text" => "m.text",
        "notice" => "m.notice",
        "emote" => "m.emote",
        other => other,
    }
}

fn message_type_from_kind(
    message_kind: &str,
    body: &str,
    use_markdown_formatting: bool,
) -> Result<(MessageType, bool), String> {
    let normalized_kind = normalized_message_kind(message_kind);
    let mut data = JsonMap::new();

    let formatted_html = formatted_html_from_markdown(body, use_markdown_formatting);
    if let Some(formatted_html) = formatted_html.as_deref() {
        data.insert(
            "format".to_owned(),
            JsonValue::String("org.matrix.custom.html".to_owned()),
        );
        data.insert(
            "formatted_body".to_owned(),
            JsonValue::String(formatted_html.to_owned()),
        );
    }

    let message_type = MessageType::new(normalized_kind, body.to_owned(), data).map_err(|e| {
        format!("failed to build matrix-sdk room message kind '{normalized_kind}': {e}")
    })?;

    Ok((message_type, formatted_html.is_some()))
}

fn formatted_html_from_markdown(body: &str, use_markdown_formatting: bool) -> Option<String> {
    if !use_markdown_formatting {
        return None;
    }

    let mut content = TextMessageEventContent::markdown(body.to_owned());
    let formatted = content.formatted.as_mut()?;
    formatted.sanitize_html(HtmlSanitizerMode::Strict, RemoveReplyFallback::No);
    let html = formatted.body.clone();
    if html_uses_only_plain_text_wrappers(&html) {
        return None;
    }

    Some(html)
}

fn html_uses_only_plain_text_wrappers(html: &str) -> bool {
    let stripped = html
        .replace("<p>", "")
        .replace("</p>\n", "")
        .replace("</p>", "")
        .replace("<br />\n", "")
        .replace("<br />", "");

    !stripped.contains('<')
}

// ---------------------------------------------------------------------------
// Message sending
// ---------------------------------------------------------------------------

pub async fn send_room_message(
    handle_id: u64,
    room_id: &str,
    body: &str,
    use_markdown_formatting: bool,
    message_kind: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let room_id = room_id.trim();
    if room_id.is_empty() {
        return Err("cannot send a matrix-sdk room message without a room id".to_owned());
    }

    let body = body.trim();

    let parsed_room_id =
        RoomId::parse(room_id).map_err(|e| format!("invalid room id '{room_id}': {e}"))?;
    let room = client
        .get_room(&parsed_room_id)
        .ok_or_else(|| format!("matrix-sdk client does not know room '{room_id}'"))?;

    if room.state() != RoomState::Joined {
        return Err(format!(
            "cannot send a matrix-sdk room message to room '{room_id}' because it is not joined"
        ));
    }

    let (message_type, has_formatted_html) =
        message_type_from_kind(message_kind, body, use_markdown_formatting)?;
    let content: AnyMessageLikeEventContent = RoomMessageEventContent::new(message_type).into();

    tracing::info!(
        handle_id,
        room_id,
        message_kind,
        has_formatted_html,
        "Queueing matrix-sdk room message"
    );

    room.send_queue()
        .send(content)
        .await
        .map_err(|e| format!("failed to queue matrix-sdk room message: {e}"))?;

    tracing::debug!(
        handle_id,
        room_id,
        message_kind,
        "Queued matrix-sdk room message"
    );

    Ok(())
}

pub async fn send_room_reply_message(
    handle_id: u64,
    room_id: &str,
    replied_to_event_id: &str,
    body: &str,
    use_markdown_formatting: bool,
    message_kind: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let replied_to_event_id = replied_to_event_id.trim();
    if replied_to_event_id.is_empty() {
        return Err("cannot send a matrix-sdk room reply without a replied-to event id".to_owned());
    }

    let body = body.trim();

    let parsed_event_id = EventId::parse(replied_to_event_id)
        .map_err(|e| format!("invalid event id '{replied_to_event_id}': {e}"))?;

    let (message_type, has_formatted_html) =
        message_type_from_kind(message_kind, body, use_markdown_formatting)?;
    let content = RoomMessageEventContentWithoutRelation::new(message_type);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        replied_to_event_id,
        message_kind,
        has_formatted_html,
        "Sending matrix-sdk room reply"
    );

    room.timeline()
        .await
        .map_err(|e| format!("failed to build matrix-sdk room timeline for reply: {e}"))?
        .send_reply(content, parsed_event_id)
        .await
        .map_err(|e| format!("failed to send matrix-sdk room reply: {e}"))?;

    tracing::debug!(
        handle_id,
        room_id = room_id.trim(),
        replied_to_event_id,
        message_kind,
        "Queued matrix-sdk room reply"
    );

    Ok(())
}

pub async fn send_room_message_like_event_json(
    handle_id: u64,
    room_id: &str,
    event_type: &str,
    content_json: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_type = event_type.trim();
    if event_type.is_empty() {
        return Err("cannot send a matrix-sdk room event without an event type".to_owned());
    }

    let content_json = content_json.trim();
    if content_json.is_empty() {
        return Err("cannot send a matrix-sdk room event without content json".to_owned());
    }

    let raw_content: Box<serde_json::value::RawValue> = serde_json::from_str(content_json)
        .map_err(|e| format!("invalid matrix room event content json: {e}"))?;
    let content = AnyMessageLikeEventContent::from_parts(event_type, raw_content.as_ref())
        .map_err(|e| format!("failed to deserialize matrix room event '{event_type}': {e}"))?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_type,
        "Queueing matrix-sdk room message-like event from raw json"
    );

    room.send_queue()
        .send(content)
        .await
        .map_err(|e| format!("failed to queue matrix room event '{event_type}': {e}"))?;

    Ok(())
}

pub async fn send_room_edit_message(
    handle_id: u64,
    room_id: &str,
    target_event_id: &str,
    body: &str,
    use_markdown_formatting: bool,
    message_kind: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let target_event_id = target_event_id.trim();
    if target_event_id.is_empty() {
        return Err("cannot send a matrix-sdk room edit without a target event id".to_owned());
    }

    let body = body.trim();
    if body.is_empty() {
        return Err("cannot send an empty matrix-sdk room edit".to_owned());
    }

    let parsed_event_id = EventId::parse(target_event_id)
        .map_err(|e| format!("invalid target event id '{target_event_id}': {e}"))?;
    let (message_type, has_formatted_html) =
        message_type_from_kind(message_kind, body, use_markdown_formatting)?;
    let content = RoomMessageEventContentWithoutRelation::new(message_type);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        target_event_id,
        message_kind,
        has_formatted_html,
        "Queueing matrix-sdk room edit"
    );

    let edit_event = room
        .make_edit_event(&parsed_event_id, EditedContent::RoomMessage(content))
        .await
        .map_err(|e| format!("failed to build matrix-sdk room edit event: {e}"))?;

    room.send_queue()
        .send(edit_event)
        .await
        .map_err(|e| format!("failed to queue matrix-sdk room edit: {e}"))?;

    tracing::debug!(
        handle_id,
        room_id = room_id.trim(),
        target_event_id,
        message_kind,
        "Queued matrix-sdk room edit"
    );

    Ok(())
}

pub async fn send_room_attachment(
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    filename: &str,
    caption: &str,
    reply_event_id: &str,
    mime_type: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let file_path = file_path.trim();
    if file_path.is_empty() {
        return Err("cannot send a matrix-sdk room attachment without a file path".to_owned());
    }

    let mime = mime_type
        .trim()
        .parse::<Mime>()
        .map_err(|e| format!("invalid attachment mime type '{mime_type}': {e}"))?;
    let data = fs::read(file_path)
        .map_err(|e| format!("failed to read attachment file '{file_path}': {e}"))?;
    let fallback_filename = Path::new(file_path)
        .file_name()
        .and_then(|name| name.to_str())
        .filter(|name| !name.is_empty())
        .ok_or_else(|| format!("attachment path '{file_path}' does not include a file name"))?;
    let filename = {
        let trimmed = filename.trim();
        if trimmed.is_empty() {
            fallback_filename.to_owned()
        } else {
            trimmed.to_owned()
        }
    };
    let caption = caption.trim();
    let reply = if reply_event_id.trim().is_empty() {
        None
    } else {
        Some(Reply {
            event_id: EventId::parse(reply_event_id.trim())
                .map_err(|e| format!("invalid reply event id '{reply_event_id}': {e}"))?,
            enforce_thread: EnforceThread::Unthreaded,
        })
    };

    let attachment_info = build_attachment_info(&mime, &data);

    let mut config = AttachmentConfig::new().info(attachment_info);
    if !caption.is_empty() {
        config = config.caption(Some(TextMessageEventContent::plain(caption)));
    }
    if let Some(reply) = reply {
        config = config.reply(Some(reply));
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        file_path,
        filename,
        has_caption = !caption.is_empty(),
        has_reply = !reply_event_id.trim().is_empty(),
        mime_type,
        file_size = data.len(),
        "Sending matrix-sdk room attachment"
    );

    room.send_attachment(filename, &mime, data, config)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to send matrix-sdk room attachment: {e}"))
}

fn build_attachment_info(mime: &Mime, data: &[u8]) -> AttachmentInfo {
    let size = Some(UInt::try_from(data.len() as u64).unwrap_or(UInt::MAX));

    if mime.type_() == mime::IMAGE {
        let (width, height, hash) = match image::load_from_memory(data) {
            Ok(img) => {
                let (w, h) = img.dimensions();
                let hash = blurhash::encode(4, 3, w, h, img.to_rgba8().as_raw())
                    .ok();
                (
                    Some(UInt::try_from(w as u64).unwrap_or(UInt::MAX)),
                    Some(UInt::try_from(h as u64).unwrap_or(UInt::MAX)),
                    hash,
                )
            }
            Err(_) => (None, None, None),
        };
        AttachmentInfo::Image(BaseImageInfo {
            width,
            height,
            size,
            blurhash: hash,
            is_animated: None,
        })
    } else if mime.type_() == mime::VIDEO {
        AttachmentInfo::Video(BaseVideoInfo {
            size,
            ..Default::default()
        })
    } else if mime.type_() == mime::AUDIO {
        AttachmentInfo::Audio(BaseAudioInfo {
            size,
            ..Default::default()
        })
    } else {
        AttachmentInfo::File(BaseFileInfo { size })
    }
}

// ---------------------------------------------------------------------------
// Reactions, redactions, read receipts, reporting
// ---------------------------------------------------------------------------

pub async fn toggle_room_reaction(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reaction_key: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot toggle a matrix-sdk room reaction without an event id".to_owned());
    }

    let reaction_key = reaction_key.trim();
    if reaction_key.is_empty() {
        return Err("cannot toggle an empty matrix-sdk room reaction".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        reaction_key,
        "Toggling matrix-sdk room reaction"
    );

    room.timeline()
        .await
        .map_err(|e| format!("failed to build matrix-sdk room timeline for reaction: {e}"))?
        .toggle_reaction(&TimelineEventItemId::EventId(parsed_event_id), reaction_key)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to toggle matrix-sdk room reaction: {e}"))
}

pub async fn redact_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot redact a matrix-sdk room event without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;
    let trimmed_reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        has_reason = trimmed_reason.is_some(),
        "Redacting matrix-sdk room event"
    );

    room.redact(&parsed_event_id, trimmed_reason.as_deref(), None)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to redact matrix-sdk room event: {e}"))
}

pub async fn mark_room_event_as_read(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot mark a matrix-sdk room event as read without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        "Marking matrix-sdk room event as read"
    );

    room.send_multiple_receipts(
        Receipts::new()
            .fully_read_marker(Some(parsed_event_id.clone()))
            .public_read_receipt(Some(parsed_event_id)),
    )
    .await
    .map_err(|e| format!("failed to mark matrix-sdk room event as read: {e}"))
}

pub async fn report_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
    score: i32,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot report a matrix-sdk room event without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;
    let trimmed_reason = trim_reason(reason);
    let clamped_score = score.clamp(
        i32::from(ReportedContentScore::MIN.value()),
        i32::from(ReportedContentScore::MAX.value()),
    );
    let reported_score = ReportedContentScore::try_from(clamped_score)
        .map_err(|_| format!("invalid reported-content score '{score}'"))?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        score = clamped_score,
        has_reason = trimmed_reason.is_some(),
        "Reporting matrix-sdk room event"
    );

    room.report_content(parsed_event_id, Some(reported_score), trimmed_reason)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to report matrix-sdk room event: {e}"))
}
