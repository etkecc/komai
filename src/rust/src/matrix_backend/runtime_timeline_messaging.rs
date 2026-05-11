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
    room::Receipts,
    room::edit::EditedContent,
    room::reply::{EnforceThread, Reply},
    ruma::{
        EventId, UInt,
        html::{HtmlSanitizerMode, RemoveReplyFallback},
        events::EventContentFromType,
        events::room::message::{
            AddMentions, FormattedBody, MessageType, RoomMessageEventContentWithoutRelation,
            TextMessageEventContent,
        },
    },
};
use image::GenericImageView;
use matrix_sdk_base::latest_event::LatestEventValue as BaseLatestEventValue;
use mime::Mime;
use serde_json::{Map as JsonMap, Value as JsonValue};
use std::{fs, path::Path};

// ---------------------------------------------------------------------------
// Diagnostic helpers
// ---------------------------------------------------------------------------

/// Stable snake_case tag for a room's cached encryption state, for inclusion
/// in send-time trace logs. Lets us diagnose the "plaintext-in-encrypted-room"
/// class of bugs (see var/plans/matrix-sdk-bump-and-cleartext-send-bug.md)
/// by grepping server-side logs for sends where the local cache reported
/// `not_encrypted` — a wire capture of a plaintext event in such a send
/// confirms the mismatch.
fn encryption_state_tag(room: &matrix_sdk::Room) -> &'static str {
    use matrix_sdk_base::EncryptionState;
    match room.encryption_state() {
        EncryptionState::Encrypted => "encrypted",
        EncryptionState::NotEncrypted => "not_encrypted",
        EncryptionState::Unknown => "unknown",
    }
}

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

/// Build a `TextMessageEventContent` for a media-attachment caption, mirroring
/// the markdown→HTML logic of regular text sends. We can't reuse
/// `message_type_from_kind` here because the attachment path takes a
/// `TextMessageEventContent` directly via `AttachmentConfig::caption`, not a
/// JSON-built `MessageType`. Element X populates `formatted_body` on
/// `m.image`/etc. the same way; receiving clients render it as HTML.
fn caption_text_content(caption: &str, use_markdown_formatting: bool) -> TextMessageEventContent {
    let mut content = TextMessageEventContent::plain(caption.to_owned());
    if let Some(html) = formatted_html_from_markdown(caption, use_markdown_formatting) {
        content.formatted = Some(FormattedBody::html(html));
    }
    content
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
    // Markdown can produce HTML whose visible text is empty even though the
    // input body wasn't (e.g. `*` → `<ul><li></li></ul>`, `# ` → `<h1></h1>`).
    // Sending such a `formatted_body` is worse than sending none at all: the
    // receiver renders the empty wrapper and loses the body. Element drops it
    // in the same situations.
    if html_visible_text_is_empty(&html) {
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

fn html_visible_text_is_empty(html: &str) -> bool {
    let mut visible = String::with_capacity(html.len());
    let mut in_tag = false;
    for ch in html.chars() {
        match ch {
            '<' => in_tag = true,
            '>' => in_tag = false,
            _ if !in_tag => visible.push(ch),
            _ => {}
        }
    }

    let decoded = visible
        .replace("&nbsp;", " ")
        .replace("&amp;", "&")
        .replace("&lt;", "<")
        .replace("&gt;", ">")
        .replace("&quot;", "\"")
        .replace("&#39;", "'");

    decoded.chars().all(char::is_whitespace)
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
        encryption_state = encryption_state_tag(&room),
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
    thread_id: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let replied_to_event_id = replied_to_event_id.trim();
    if replied_to_event_id.is_empty() {
        return Err("cannot send a matrix-sdk room reply without a replied-to event id".to_owned());
    }

    let body = body.trim();
    let thread_id = thread_id.trim();

    let parsed_event_id = EventId::parse(replied_to_event_id)
        .map_err(|e| format!("invalid event id '{replied_to_event_id}': {e}"))?;

    let (message_type, has_formatted_html) =
        message_type_from_kind(message_kind, body, use_markdown_formatting)?;
    let content = RoomMessageEventContentWithoutRelation::new(message_type);

    let is_threaded = !thread_id.is_empty();

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        replied_to_event_id,
        message_kind,
        has_formatted_html,
        is_threaded,
        encryption_state = encryption_state_tag(&room),
        "Sending matrix-sdk room reply"
    );

    // We construct the reply event and hand it directly to the send queue
    // instead of using `Timeline::send_reply`, which would rebuild a fresh
    // `Timeline` per send (event-cache subscribe, encryption-state lookup,
    // etc.) — those `.await`s can stall on a degraded network (e.g. just
    // after a suspend/resume) and leave the reply with no local echo.
    let enforce_thread = if is_threaded {
        let is_reply_within_thread = replied_to_event_id != thread_id;
        let reply_within_thread = if is_reply_within_thread {
            matrix_sdk::ruma::events::room::message::ReplyWithinThread::Yes
        } else {
            matrix_sdk::ruma::events::room::message::ReplyWithinThread::No
        };
        EnforceThread::Threaded(reply_within_thread)
    } else {
        EnforceThread::MaybeThreaded
    };
    let reply = Reply {
        event_id: parsed_event_id,
        enforce_thread,
        add_mentions: AddMentions::Yes,
    };
    let reply_content = room.make_reply_event(content, reply)
        .await
        .map_err(|e| format!("failed to build matrix-sdk reply event: {e}"))?;
    room.send_queue()
        .send(reply_content.into())
        .await
        .map_err(|e| format!("failed to queue matrix-sdk reply: {e}"))?;

    tracing::debug!(
        handle_id,
        room_id = room_id.trim(),
        replied_to_event_id,
        message_kind,
        is_threaded,
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
        encryption_state = encryption_state_tag(&room),
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
        encryption_state = encryption_state_tag(&room),
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

#[allow(clippy::too_many_arguments)]
pub async fn send_room_attachment(
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    filename: &str,
    caption: &str,
    use_markdown_formatting: bool,
    reply_event_id: &str,
    thread_id: &str,
    mime_type: &str,
    duration_ms: u64,
    is_voice: bool,
    waveform: &[f32],
    strip_image_metadata: bool,
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
    let data = if strip_image_metadata {
        crate::matrix_backend::image_metadata::strip_image_metadata(data, &mime)
    } else {
        data
    };
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
    let thread_id = thread_id.trim();
    let reply = if reply_event_id.trim().is_empty() && thread_id.is_empty() {
        None
    } else {
        // When in a thread but no specific reply, reply to the thread root.
        let effective_reply_event_id = if reply_event_id.trim().is_empty() {
            thread_id
        } else {
            reply_event_id.trim()
        };
        let enforce_thread = if thread_id.is_empty() {
            EnforceThread::MaybeThreaded
        } else {
            let is_reply_within_thread = effective_reply_event_id != thread_id;
            let reply_within_thread = if is_reply_within_thread {
                matrix_sdk::ruma::events::room::message::ReplyWithinThread::Yes
            } else {
                matrix_sdk::ruma::events::room::message::ReplyWithinThread::No
            };
            EnforceThread::Threaded(reply_within_thread)
        };
        Some(Reply {
            event_id: EventId::parse(effective_reply_event_id)
                .map_err(|e| format!("invalid reply event id '{effective_reply_event_id}': {e}"))?,
            enforce_thread,
            add_mentions: AddMentions::Yes,
        })
    };

    let attachment_info = build_attachment_info(&mime, &data, duration_ms, is_voice, waveform);

    let mut config = AttachmentConfig::new().info(attachment_info);
    let caption_content = if caption.is_empty() {
        None
    } else {
        Some(caption_text_content(caption, use_markdown_formatting))
    };
    let caption_has_formatted_html = caption_content
        .as_ref()
        .is_some_and(|c| c.formatted.is_some());
    if let Some(content) = caption_content {
        config = config.caption(Some(content));
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
        caption_has_formatted_html,
        has_reply = !reply_event_id.trim().is_empty(),
        mime_type,
        file_size = data.len(),
        encryption_state = encryption_state_tag(&room),
        "Sending matrix-sdk room attachment"
    );

    room.send_attachment(filename, &mime, data, config)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to send matrix-sdk room attachment: {e}"))
}

fn build_attachment_info(
    mime: &Mime,
    data: &[u8],
    duration_ms: u64,
    is_voice: bool,
    waveform: &[f32],
) -> AttachmentInfo {
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
        let duration = if duration_ms > 0 {
            Some(std::time::Duration::from_millis(duration_ms))
        } else {
            None
        };
        let waveform_data = if waveform.is_empty() {
            None
        } else {
            Some(waveform.to_vec())
        };
        let audio_info = BaseAudioInfo {
            size,
            duration,
            waveform: waveform_data,
        };
        if is_voice {
            AttachmentInfo::Voice(audio_info)
        } else {
            AttachmentInfo::Audio(audio_info)
        }
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
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot toggle a matrix-sdk room reaction without an event id".to_owned());
    }

    let reaction_key = reaction_key.trim();
    if reaction_key.is_empty() {
        return Err("cannot toggle an empty matrix-sdk room reaction".to_owned());
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        reaction_key,
        "Toggling matrix-sdk room reaction"
    );

    // Dispatch to the active room timeline loop via its command channel.
    // The active timeline already has items loaded (including existing reactions),
    // so toggle_reaction can correctly detect whether to add or redact.
    // Building a fresh timeline via room.timeline() would lack this state
    // and fail to find existing reactions to remove.
    let command_sender = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles
            .get(&handle_id)
            .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;
        let active_room = handle.active_room_id.as_ref().ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} has no active room")
        })?;
        let task = handle.room_timeline_tasks.get(active_room).ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no timeline task for active room '{active_room}'"
            )
        })?;
        if task.thread.is_finished() {
            return Err(format!(
                "matrix-sdk active room timeline task for '{}' is no longer running",
                task.room_id
            ));
        }
        task.commands.clone()
    };

    let (response_tx, response_rx) = tokio::sync::oneshot::channel();
    command_sender
        .send(MatrixBackendRoomTimelineCommand::ToggleReaction {
            event_id: event_id.to_owned(),
            reaction_key: reaction_key.to_owned(),
            response: response_tx,
        })
        .map_err(|_| "failed to send toggle-reaction command to active room timeline".to_owned())?;

    response_rx
        .await
        .map_err(|_| "active room timeline dropped the toggle-reaction response".to_owned())?
}

/// Cancel a failed / pending local echo identified by its transaction id.
///
/// Dispatches to the active room timeline loop so we can reach the persisted
/// `SendHandle`. Returns `Ok(true)` when the send-queue entry was aborted,
/// `Ok(false)` when there was nothing to abort (already sent / gone), and
/// an error when the item cannot be located or has no send handle.
pub async fn cancel_local_echo(
    handle_id: u64,
    room_id: &str,
    transaction_id: &str,
) -> Result<bool, String> {
    let transaction_id = transaction_id.trim();
    if transaction_id.is_empty() {
        return Err("cannot cancel a local echo without a transaction id".to_owned());
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        transaction_id,
        "Cancelling matrix-sdk local echo"
    );

    let command_sender = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles
            .get(&handle_id)
            .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;
        let active_room = handle.active_room_id.as_ref().ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} has no active room")
        })?;
        let task = handle.room_timeline_tasks.get(active_room).ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no timeline task for active room '{active_room}'"
            )
        })?;
        if task.thread.is_finished() {
            return Err(format!(
                "matrix-sdk active room timeline task for '{}' is no longer running",
                task.room_id
            ));
        }
        task.commands.clone()
    };

    let (response_tx, response_rx) = tokio::sync::oneshot::channel();
    command_sender
        .send(MatrixBackendRoomTimelineCommand::CancelLocalEcho {
            transaction_id: transaction_id.to_owned(),
            response: response_tx,
        })
        .map_err(|_| {
            "failed to send cancel-local-echo command to active room timeline".to_owned()
        })?;

    response_rx
        .await
        .map_err(|_| "active room timeline dropped the cancel-local-echo response".to_owned())?
}

/// Retry (unwedge) a wedged local echo identified by its transaction id.
///
/// Dispatches to the active room timeline loop so we can reach the persisted
/// `SendHandle`. The SDK keeps the item in the queue and marks it for
/// another send attempt — the timeline subscription will emit either a
/// successful `Sent` update or another `SendingFailed` once the retry
/// resolves.
pub async fn retry_local_echo(
    handle_id: u64,
    room_id: &str,
    transaction_id: &str,
) -> Result<(), String> {
    let transaction_id = transaction_id.trim();
    if transaction_id.is_empty() {
        return Err("cannot retry a local echo without a transaction id".to_owned());
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        transaction_id,
        "Retrying matrix-sdk local echo"
    );

    let command_sender = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles
            .get(&handle_id)
            .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;
        let active_room = handle.active_room_id.as_ref().ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} has no active room")
        })?;
        let task = handle.room_timeline_tasks.get(active_room).ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no timeline task for active room '{active_room}'"
            )
        })?;
        if task.thread.is_finished() {
            return Err(format!(
                "matrix-sdk active room timeline task for '{}' is no longer running",
                task.room_id
            ));
        }
        task.commands.clone()
    };

    let (response_tx, response_rx) = tokio::sync::oneshot::channel();
    command_sender
        .send(MatrixBackendRoomTimelineCommand::RetryLocalEcho {
            transaction_id: transaction_id.to_owned(),
            response: response_tx,
        })
        .map_err(|_| {
            "failed to send retry-local-echo command to active room timeline".to_owned()
        })?;

    response_rx
        .await
        .map_err(|_| "active room timeline dropped the retry-local-echo response".to_owned())?
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
    public_receipt: bool,
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
        public_receipt,
        "Marking matrix-sdk room event as read"
    );

    // When the user has opted out of advertising read state, we still need the
    // homeserver to consider the room read for *this* user (so unread counts
    // clear, `m.fully_read` advances, `m.marked_unread` is cleared) — we just
    // switch the ephemeral receipt from public to private so it isn't
    // federated outward or surfaced to other users via /sync.
    let mut receipts = Receipts::new().fully_read_marker(Some(parsed_event_id.clone()));
    if public_receipt {
        receipts = receipts.public_read_receipt(Some(parsed_event_id.clone()));
    } else {
        receipts = receipts.private_read_receipt(Some(parsed_event_id.clone()));
    }

    room.send_multiple_receipts(receipts)
        .await
        .map_err(|e| format!("failed to mark matrix-sdk room event as read: {e}"))?;

    // Optimistically anchor `read_receipts.latest_active` to the just-acked
    // event and zero the counts.  matrix-sdk's `Room::send_multiple_receipts`
    // returns once the HTTP request is acknowledged but never touches local
    // state — it relies on sliding sync to echo the receipt back through
    // the receipts extension, at which point `compute_unread_counts`
    // recomputes the active receipt and the unread counts.
    //
    // In the wild we've seen rooms get stuck: HTTP returns 200, so the
    // server has the receipt, but the local `latest_active` never moves
    // past some old event and the badge keeps reappearing every time
    // `compute_unread_counts` runs.  We don't fully understand the
    // trigger — receipts work fine for most rooms, so this isn't a
    // blanket "Synapse never echoes own receipts" problem.  It might be
    // a one-shot dropped echo, or specific to rooms where the latest
    // events are state events (e.g. `m.room.server_acl` churn from a
    // moderation bot), or something else entirely.  Posting a fresh
    // message clears it via the implicit-receipt path in matrix-sdk's
    // `select_best_receipt` (events we sent count as receipts on
    // themselves), but explicit `m.read` does not have that fallback.
    //
    // Whatever the trigger, the HTTP just returned 200 so the server
    // has the receipt; mirror that into RoomInfo (with
    // `RoomInfoNotableUpdateReasons::READ_RECEIPT` so observers
    // update) and persist via `state_store().save_changes` (otherwise
    // the stale state reloads on next startup).  Future syncs still
    // recompute correctly: any new event past `parsed_event_id` is
    // counted from this anchor by `find_and_process_events`.  For rooms
    // that weren't stuck this is a no-op once sync arrives with the
    // real echo.
    {
        use matrix_sdk_base::{
            RoomInfoNotableUpdateReasons, StateChanges, read_receipts::LatestReadReceipt,
        };

        let mut info = room.clone_info();
        let mut receipts = info.read_receipts().clone();
        receipts.latest_active = Some(LatestReadReceipt { event_id: parsed_event_id });
        receipts.num_unread = 0;
        receipts.num_notifications = 0;
        receipts.num_mentions = 0;
        info.set_read_receipts(receipts);

        // matrix-sdk's `send_multiple_receipts` internally calls `set_unread_flag(false)`
        // after posting the receipt, but that — like our explicit `mark_room_unread` —
        // only POSTs the account-data write.  Local `is_marked_unread` would otherwise
        // stay `true` until the sliding-sync echo, leaving a manually-marked-unread
        // room visually stuck after the user opens it.  Mirror the clear locally too.
        if matrix_sdk_base::Room::is_marked_unread(&room)
            && let Err(error) = patch_marked_unread(&mut info, false)
        {
            tracing::warn!(
                room_id = room_id.trim(),
                %error,
                "Failed to apply optimistic marked-unread clear; UI will refresh on the next sync echo"
            );
        }

        let mut state_changes = StateChanges::default();
        state_changes.add_room(info.clone());
        if let Err(error) = room.client().state_store().save_changes(&state_changes).await {
            tracing::warn!(
                room_id = room_id.trim(),
                %error,
                "Failed to persist optimistic read-receipt update; \
                 in-memory state is still applied, but the badge may \
                 reappear after restart"
            );
        }

        room.update_room_info(|_| (info, RoomInfoNotableUpdateReasons::READ_RECEIPT)).await;
    }

    Ok(())
}

// `RoomInfo::base_info::is_marked_unread` is `pub(crate)` on matrix-sdk-base,
// so we can't mutate it through field access from this crate.  RoomInfo derives
// `Serialize`+`Deserialize` (used for state-store persistence), so a JSON
// round-trip is the lightest path to an optimistic local update without
// forking matrix-sdk.  Setting `is_marked_unread_source` to `Stable` matches
// the content type matrix-sdk's own `set_unread_flag` writes (the stable
// `m.marked_unread` event), and aligns with `on_unread_marker`'s rule that a
// stable source can't be downgraded by an unstable echo.
fn patch_marked_unread(
    info: &mut matrix_sdk_base::RoomInfo,
    unread: bool,
) -> Result<(), String> {
    let mut json = serde_json::to_value(&*info).map_err(|e| {
        format!("failed to serialize RoomInfo for optimistic marked-unread update: {e}")
    })?;
    let base = json
        .get_mut("base_info")
        .and_then(JsonValue::as_object_mut)
        .ok_or_else(|| "RoomInfo serialization missing base_info object".to_owned())?;
    base.insert("is_marked_unread".to_owned(), JsonValue::Bool(unread));
    base.insert(
        "is_marked_unread_source".to_owned(),
        JsonValue::String("Stable".to_owned()),
    );
    *info = serde_json::from_value(json).map_err(|e| {
        format!("failed to deserialize RoomInfo after optimistic marked-unread update: {e}")
    })?;
    Ok(())
}

async fn optimistically_flip_marked_unread(
    room: &matrix_sdk::Room,
    room_id: &str,
    unread: bool,
) {
    use matrix_sdk_base::{RoomInfoNotableUpdateReasons, StateChanges};

    let mut info = room.clone_info();
    if let Err(error) = patch_marked_unread(&mut info, unread) {
        tracing::warn!(
            room_id = room_id.trim(),
            %error,
            "Failed to apply optimistic marked-unread update; UI will refresh on the next sync echo"
        );
        return;
    }

    let mut state_changes = StateChanges::default();
    state_changes.add_room(info.clone());
    if let Err(error) = room.client().state_store().save_changes(&state_changes).await {
        tracing::warn!(
            room_id = room_id.trim(),
            %error,
            "Failed to persist optimistic marked-unread update; \
             in-memory state is still applied, but the flag may \
             revert after restart"
        );
    }

    room.update_room_info(|_| (info, RoomInfoNotableUpdateReasons::UNREAD_MARKER)).await;
}

pub async fn mark_room_as_read(
    handle_id: u64,
    room_id: &str,
    public_receipt: bool,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    // UFCS to the synchronous matrix_sdk_base inherent (mirrors the call in
    // runtime_room_list.rs); avoids dispatching to the async UI extension trait.
    let latest_event: BaseLatestEventValue = matrix_sdk_base::Room::latest_event(&room);
    let event_id = latest_event.event_id().map(|id| id.to_string()).ok_or_else(|| {
        format!(
            "matrix-sdk room {} has no known latest event to anchor a read receipt on",
            room_id.trim()
        )
    })?;

    mark_room_event_as_read(handle_id, room_id, &event_id, public_receipt).await
}

pub async fn mark_room_unread(handle_id: u64, room_id: &str, unread: bool) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        unread,
        "Setting matrix-sdk room marked-unread flag"
    );

    room.set_unread_flag(unread)
        .await
        .map_err(|e| format!("failed to set marked-unread flag: {e}"))?;

    // matrix-sdk's `set_unread_flag` only POSTs the account-data write; local
    // `RoomInfo.is_marked_unread` updates only when the server echoes via the
    // next sliding-sync round.  Mirror the value locally so the UI refreshes
    // immediately, matching the optimistic pattern in `mark_room_event_as_read`.
    optimistically_flip_marked_unread(&room, room_id, unread).await;

    Ok(())
}

pub async fn report_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    reason: &str,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot report a matrix-sdk room event without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;
    let trimmed_reason = trim_reason(reason);

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        has_reason = trimmed_reason.is_some(),
        "Reporting matrix-sdk room event"
    );

    room.report_content(parsed_event_id, trimmed_reason)
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to report matrix-sdk room event: {e}"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn html_visible_text_is_empty_detects_empty_list() {
        assert!(html_visible_text_is_empty("<ul>\n<li></li>\n</ul>\n"));
        assert!(html_visible_text_is_empty("<h1></h1>"));
        assert!(html_visible_text_is_empty("<hr />"));
        assert!(html_visible_text_is_empty("<blockquote>\n</blockquote>"));
    }

    #[test]
    fn html_visible_text_is_empty_keeps_real_content() {
        assert!(!html_visible_text_is_empty("<ul><li>foo</li></ul>"));
        assert!(!html_visible_text_is_empty("<h1>title</h1>"));
        assert!(!html_visible_text_is_empty("<p><strong>bold</strong></p>"));
    }

    #[test]
    fn html_visible_text_is_empty_decodes_entities() {
        assert!(!html_visible_text_is_empty("<p>&lt;</p>"));
        assert!(!html_visible_text_is_empty("<p>&amp;</p>"));
    }

    #[test]
    fn formatted_html_skipped_for_lone_asterisk() {
        // Regression for https://github.com/etkecc/komai/issues/104:
        // pulldown-cmark turns `*` into `<ul><li></li></ul>`, which is
        // worthless as `formatted_body` and would hide the body on render.
        assert_eq!(formatted_html_from_markdown("*", true), None);
    }

    #[test]
    fn formatted_html_skipped_for_other_visibly_empty_markdown() {
        assert_eq!(formatted_html_from_markdown("# ", true), None);
        assert_eq!(formatted_html_from_markdown("***", true), None);
        assert_eq!(formatted_html_from_markdown("- ", true), None);
    }

    #[test]
    fn formatted_html_skipped_for_plain_text() {
        // Existing guard: a paragraph wrapper around plain text is no improvement.
        assert_eq!(formatted_html_from_markdown("hello world", true), None);
    }

    #[test]
    fn formatted_html_kept_for_real_markdown() {
        assert!(formatted_html_from_markdown("**bold**", true).is_some());
        assert!(formatted_html_from_markdown("* foo", true).is_some());
        assert!(formatted_html_from_markdown("- a\n- b", true).is_some());
        assert!(formatted_html_from_markdown("## heading", true).is_some());
    }

    #[test]
    fn formatted_html_disabled_when_markdown_off() {
        assert_eq!(formatted_html_from_markdown("**bold**", false), None);
    }

    #[test]
    fn caption_carries_formatted_body_for_real_markdown() {
        let content = caption_text_content("**bold** [link](https://example.com)", true);
        assert_eq!(content.body, "**bold** [link](https://example.com)");
        let formatted = content.formatted.expect("expected formatted_body");
        assert!(formatted.body.contains("<strong>bold</strong>"));
        assert!(formatted.body.contains("href=\"https://example.com\""));
    }

    #[test]
    fn caption_omits_formatted_body_when_markdown_off() {
        let content = caption_text_content("**bold**", false);
        assert!(content.formatted.is_none());
    }

    #[test]
    fn caption_omits_formatted_body_for_plain_text_when_markdown_on() {
        // Same guard as text messages: a paragraph wrapper around plain text
        // doesn't add value, so we don't pay the formatted_body cost.
        let content = caption_text_content("hello world", true);
        assert!(content.formatted.is_none());
    }
}
