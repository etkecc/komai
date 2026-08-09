// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Send timeline messages: plain text, replies, edits, attachments, and
//! the raw m.room.message-like JSON path used by bridges/baibot.

use super::*;

use matrix_sdk::ruma::{OwnedUserId, UserId, events::Mentions};
use std::collections::BTreeSet;

/// Build the `m.mentions` content (MSC3952 intentional mentions) from the
/// composer's tracked mentions. `mention_user_ids` is a newline-separated list
/// of user MXIDs (the format the C++ bridge sends); invalid entries are
/// skipped. Returns `None` when there is nothing to mention so callers can
/// avoid attaching an empty `m.mentions`.
pub(super) fn build_mentions(mention_user_ids: &str, mentions_room: bool) -> Option<Mentions> {
    let mut user_ids: BTreeSet<OwnedUserId> = BTreeSet::new();
    for raw in mention_user_ids.split('\n') {
        let candidate = raw.trim();
        if candidate.is_empty() {
            continue;
        }
        match UserId::parse(candidate) {
            Ok(user_id) => {
                user_ids.insert(user_id);
            }
            Err(error) => {
                tracing::warn!(candidate, %error, "Ignoring invalid mention user id");
            }
        }
    }

    if user_ids.is_empty() && !mentions_room {
        return None;
    }

    let mut mentions = Mentions::new();
    mentions.user_ids = user_ids;
    mentions.room = mentions_room;
    Some(mentions)
}

pub(super) fn encryption_state_tag(room: &matrix_sdk::Room) -> &'static str {
    use matrix_sdk_base::EncryptionState;
    match room.encryption_state() {
        EncryptionState::Encrypted => "encrypted",
        EncryptionState::NotEncrypted => "not_encrypted",
        EncryptionState::Unknown => "unknown",
    }
}

pub(super) fn normalized_message_kind(message_kind: &str) -> &str {
    match message_kind.trim() {
        "message" | "text" => "m.text",
        "notice" => "m.notice",
        "emote" => "m.emote",
        other => other,
    }
}

pub(super) fn message_type_from_kind(
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

pub(super) fn caption_text_content(caption: &str, use_markdown_formatting: bool) -> TextMessageEventContent {
    let mut content = TextMessageEventContent::plain(caption.to_owned());
    if let Some(html) = formatted_html_from_markdown(caption, use_markdown_formatting) {
        content.formatted = Some(FormattedBody::html(html));
    }
    content
}

pub(super) fn formatted_html_from_markdown(body: &str, use_markdown_formatting: bool) -> Option<String> {
    if !use_markdown_formatting {
        return None;
    }

    let mut formatted = FormattedBody::html(markdown_to_html_treating_raw_html_as_text(body)?);
    formatted.sanitize_html(HtmlSanitizerMode::Strict, RemoveReplyFallback::No);
    let html = formatted.body;
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

/// Markdown-to-HTML mirroring ruma's `FormattedBody::markdown`, except raw
/// HTML in the input is demoted to literal text instead of passing through as
/// markup. CommonMark treats a typed `<pre>` as the start of a real HTML
/// block that swallows the rest of the message for every recipient; in a chat
/// composer, tag-like tokens are almost always meant literally. Backtick and
/// fenced code plus `<https://...>` autolinks are unaffected (they never
/// parse as raw HTML). Returns `None` when the input contains no markdown
/// formatting, matching ruma (and Element, which sends no `formatted_body`
/// for raw-HTML-only input).
fn markdown_to_html_treating_raw_html_as_text(text: &str) -> Option<String> {
    use pulldown_cmark::{CowStr, Event, Options, Parser, Tag, TagEnd};

    const OPTIONS: Options = Options::ENABLE_TABLES.union(Options::ENABLE_STRIKETHROUGH);

    // Emits raw HTML as escaped-later Text events, converting embedded
    // newlines to hard breaks so multi-line raw blocks don't collapse into
    // one rendered line.
    fn push_literal_text<'a>(events: &mut Vec<Event<'a>>, raw: CowStr<'a>) {
        let mut first = true;
        for line in raw.split('\n') {
            if !first {
                events.push(Event::HardBreak);
            }
            first = false;
            let line = line.strip_suffix('\r').unwrap_or(line);
            if !line.is_empty() {
                events.push(Event::Text(line.to_owned().into()));
            }
        }
    }

    fn is_block_tag(tag: &Tag<'_>) -> bool {
        matches!(
            tag,
            Tag::Paragraph
                | Tag::Heading { .. }
                | Tag::BlockQuote(_)
                | Tag::CodeBlock(_)
                | Tag::HtmlBlock
                | Tag::List(_)
                | Tag::FootnoteDefinition(_)
                | Tag::Table(_)
        )
    }

    let mut parser_events: Vec<Event<'_>> = Vec::new();
    for event in Parser::new_ext(text, OPTIONS) {
        match event {
            Event::SoftBreak => parser_events.push(Event::HardBreak),
            Event::Html(raw) | Event::InlineHtml(raw) => {
                push_literal_text(&mut parser_events, raw);
            }
            // With their contents demoted to text these wrappers carry no
            // information, and keeping them would make the plain-text walk
            // below see phantom block structure.
            Event::Start(Tag::HtmlBlock) | Event::End(TagEnd::HtmlBlock) => {}
            other => parser_events.push(other),
        }
    }

    // The rest mirrors ruma's `parse_markdown`: figure out whether the events
    // amount to more than the original text plus newlines, bail out when they
    // don't, and unwrap the single wrapping paragraph of inline-only content.
    let first_event_is_paragraph_start = parser_events
        .first()
        .is_some_and(|event| matches!(event, Event::Start(Tag::Paragraph)));
    let last_event_is_paragraph_end = parser_events
        .last()
        .is_some_and(|event| matches!(event, Event::End(TagEnd::Paragraph)));
    let mut is_inline = first_event_is_paragraph_start && last_event_is_paragraph_end;
    let mut has_markdown = !is_inline;

    if !has_markdown {
        // If the string contains no markdown, the only change should be
        // newlines becoming hard breaks: check that by finding all other
        // characters of the original string in the text events.
        let mut pos = 0;
        for event in parser_events.iter().skip(1) {
            match event {
                Event::Text(s) if text[pos..].starts_with(s.as_ref()) => {
                    pos += s.len();
                    continue;
                }
                Event::HardBreak => {
                    if text[pos..].starts_with("\r\n") {
                        pos += 2;
                        continue;
                    } else if text[pos..].starts_with(['\r', '\n']) {
                        pos += 1;
                        continue;
                    }
                }
                Event::End(TagEnd::Paragraph) => continue,
                Event::Start(tag) => {
                    is_inline &= !is_block_tag(tag);
                }
                _ => {}
            }

            has_markdown = true;
            if !is_inline {
                break;
            }
        }
        has_markdown |= pos != text.len();
    }

    if !has_markdown {
        return None;
    }

    let mut events_iter = parser_events.into_iter();
    if is_inline {
        events_iter.next();
        events_iter.next_back();
    }

    let mut html_body = String::new();
    pulldown_cmark::html::push_html(&mut html_body, events_iter);
    Some(html_body)
}

pub(super) fn html_uses_only_plain_text_wrappers(html: &str) -> bool {
    let stripped = html
        .replace("<p>", "")
        .replace("</p>\n", "")
        .replace("</p>", "")
        .replace("<br />\n", "")
        .replace("<br />", "")
        // The ruma sanitizer serializes hard breaks as `<br>`.
        .replace("<br>\n", "")
        .replace("<br>", "");

    !stripped.contains('<')
}

pub(super) fn html_visible_text_is_empty(html: &str) -> bool {
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

// Message sending
// ---------------------------------------------------------------------------

pub async fn send_room_message(
    handle_id: u64,
    room_id: &str,
    body: &str,
    use_markdown_formatting: bool,
    message_kind: &str,
    mention_user_ids: &str,
    mentions_room: bool,
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
    let message_content = RoomMessageEventContent::new(message_type);
    let mentions = build_mentions(mention_user_ids, mentions_room);
    let has_mentions = mentions.is_some();
    let message_content = match mentions {
        Some(mentions) => message_content.add_mentions(mentions),
        None => message_content,
    };
    let content: AnyMessageLikeEventContent = message_content.into();

    tracing::info!(
        handle_id,
        room_id,
        message_kind,
        has_formatted_html,
        has_mentions,
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
    mention_user_ids: &str,
    mentions_room: bool,
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
    // The replied-to sender is added by `AddMentions::Yes` below; here we add
    // the mentions the composer tracked from the body (user pills and @room).
    let content = match build_mentions(mention_user_ids, mentions_room) {
        Some(mentions) => content.add_mentions(mentions),
        None => content,
    };

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
    mention_user_ids: &str,
    mentions_room: bool,
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
    let content = match build_mentions(mention_user_ids, mentions_room) {
        Some(mentions) => content.add_mentions(mentions),
        None => content,
    };

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

pub(super) fn build_attachment_info(
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
