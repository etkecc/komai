// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use super::event_summary::summarize_timeline_content;
use matrix_sdk::{
    attachment::AttachmentConfig,
    room::{Receipts, ReportedContentScore},
    room::edit::EditedContent,
    room::reply::{EnforceThread, Reply},
    ruma::{
        EventId,
        html::{HtmlSanitizerMode, RemoveReplyFallback},
        events::EventContentFromType,
        events::receipt::{ReceiptThread, ReceiptType},
        events::room::pinned_events::RoomPinnedEventsEventContent,
        events::room::message::{
            MessageType, RoomMessageEventContentWithoutRelation, TextMessageEventContent,
        },
    },
};
use mime::Mime;
use serde_json::{Map as JsonMap, Value as JsonValue};
use std::{
    fs,
    path::Path,
    sync::OnceLock,
    time::{Duration as StdDuration, Instant},
};

fn is_truthy_env_value(name: &str) -> bool {
    std::env::var_os(name).is_some_and(|value| {
        let value = value.to_string_lossy();
        !matches!(
            value.as_ref(),
            "" | "0" | "false" | "False" | "FALSE" | "no" | "No" | "NO" | "off" | "Off"
                | "OFF"
        )
    })
}

fn room_switch_perf_enabled() -> bool {
    static ENABLED: OnceLock<bool> = OnceLock::new();
    *ENABLED.get_or_init(|| {
        is_truthy_env_value("KOMAI_ROOM_SWITCH_PERF")
            || is_truthy_env_value("KOMAI_PERF_ROOM_SWITCH")
    })
}

fn log_room_timeline_perf(
    handle_id: u64,
    room_id: &str,
    phase: &str,
    elapsed: StdDuration,
    extra: &str,
) {
    if !room_switch_perf_enabled() {
        return;
    }

    tracing::info!(
        "[room-switch-perf] phase={} handle_id={} room_id={} elapsed_us={} elapsed_ms={:.3}{}",
        phase,
        handle_id,
        room_id,
        elapsed.as_micros(),
        elapsed.as_secs_f64() * 1000.0,
        extra
    );
}

/// After the timeline loads events for a room whose room-list entry still
/// has `timestamp == 0` (no cached latest event from sliding sync), backfill
/// the entry's data in the Rust-side room-list snapshot and notify C++ via
/// the targeted preview-update path (no model reset, preserves scroll).
///
/// This works around a matrix-sdk limitation where network backward
/// pagination does not send `RoomEventCacheGenericUpdate`, so
/// `new_latest_event` / `new_latest_event_timestamp()` remain `None`.
fn maybe_backfill_room_list_preview(
    handle_id: u64,
    room_id: &str,
    timeline_snapshot: &[MatrixTimelineItem],
) {
    if timeline_snapshot.is_empty() {
        return;
    }

    // Find the newest event by timestamp.  Accept any event-like item
    // (including encrypted ones) — exclude only virtual items and state events.
    let is_state_like = |kind: &str| {
        matches!(
            kind,
            "other_state" | "failed_to_parse_state" | "membership_change"
        )
    };
    let Some(newest) = timeline_snapshot
        .iter()
        .filter(|item| item.timestamp > 0 && !item.event_id.is_empty() && !is_state_like(&item.item_kind))
        .max_by_key(|item| item.timestamp)
    else {
        return;
    };

    let handles = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex");
    let Some(handle) = handles.get(&handle_id) else {
        return;
    };
    let mut room_list = handle
        .room_list_snapshot
        .lock()
        .expect("poisoned matrix room-list snapshot mutex");

    let Some(entry) = room_list.iter_mut().find(|r| r.room_id == room_id) else {
        return;
    };
    if entry.timestamp > 0 {
        return; // Already has a real timestamp — nothing to backfill.
    }

    entry.timestamp = newest.timestamp;
    entry.last_message = newest.body.clone();
    entry.last_message_kind = newest.matrix_event_type.clone();
    entry.latest_event_id = newest.event_id.clone();

    drop(room_list);
    drop(handles);

    tracing::info!(
        handle_id,
        room_id,
        timestamp = newest.timestamp,
        "Backfilled room-list preview from timeline"
    );

    // Targeted notification — updates the C++ model without resetting it.
    crate::ffi::matrix_notify_room_previews_backfilled(
        handle_id,
        vec![crate::ffi::MatrixRoomPreviewUpdate {
            room_id: room_id.to_owned(),
            latest_event_id: newest.event_id.clone(),
            last_message: newest.body.clone(),
            last_message_kind: newest.matrix_event_type.clone(),
            timestamp: newest.timestamp,
        }],
    );
}

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

pub fn select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<(), String> {
    let room_id = room_id.trim();

    let (
        client,
        room_timeline_snapshot,
        room_timeline_media_lookup,
        previous_task,
        generation,
        initial_page_size,
    ) = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get_mut(&handle_id) else {
            return Err(format!("matrix-sdk backend runtime handle {handle_id} is not active"));
        };

        if let Some(task) = handle.room_timeline_task.as_ref() {
            if task.room_id == room_id && !task.thread.is_finished() {
                tracing::debug!(
                    handle_id,
                    room_id,
                    "Matrix-sdk room timeline task is already running for the active room"
                );
                return Ok(());
            }
        }

        let previous_task = handle.room_timeline_task.take();
        let generation = handle
            .room_timeline_generation
            .fetch_add(1, Ordering::Release)
            + 1;
        handle
            .room_timeline_snapshot
            .lock()
            .expect("poisoned matrix room timeline snapshot mutex")
            .clear();
        handle
            .room_timeline_media_lookup
            .lock()
            .expect("poisoned matrix room timeline media lookup mutex")
            .clear();

        (
            handle.client.clone(),
            Arc::clone(&handle.room_timeline_snapshot),
            Arc::clone(&handle.room_timeline_media_lookup),
            previous_task,
            generation,
            handle.preferred_room_timeline_initial_page_size,
        )
    };

    if let Some(previous_task) = previous_task {
        std::thread::spawn(move || stop_room_timeline_task(handle_id, previous_task));
    }

    if room_id.is_empty() {
        tracing::info!(handle_id, "Cleared active matrix-sdk room timeline selection");
        return Ok(());
    }

    let stop_requested = Arc::new(AtomicBool::new(false));
    let stop_requested_for_thread = Arc::clone(&stop_requested);
    let (command_sender, command_receiver) = mpsc::unbounded_channel();
    let room_id_owned = room_id.to_owned();
    let room_id_for_thread = room_id_owned.clone();
    let generation_counter = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        handles
            .get(&handle_id)
            .map(|h| Arc::clone(&h.room_timeline_generation))
            .expect("handle must exist after select_active_room_timeline setup")
    };
    let room_timeline_task = std::thread::spawn(move || {
        crate::matrix_backend::ffi::runtime().block_on(run_room_timeline_loop(
            handle_id,
            generation,
            generation_counter,
            client,
            room_id_for_thread,
            initial_page_size,
            room_timeline_snapshot,
            room_timeline_media_lookup,
            command_receiver,
            stop_requested_for_thread,
        ));
    });

    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .entry(handle_id)
        .and_modify(|handle| {
            handle.room_timeline_task = Some(MatrixBackendRoomTimelineTask {
                room_id: room_id_owned.clone(),
                commands: command_sender,
                stop_requested,
                thread: room_timeline_task,
            });
        });

    tracing::info!(
        handle_id,
        room_id = %room_id_owned,
        "Started matrix-sdk room timeline task"
    );
    Ok(())
}

pub fn set_active_room_timeline_initial_page_size(
    handle_id: u64,
    page_size: u16,
) -> Result<(), String> {
    let mut handles = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex");
    let Some(handle) = handles.get_mut(&handle_id) else {
        return Err(format!("matrix-sdk backend runtime handle {handle_id} is not active"));
    };

    let clamped_page_size = page_size.clamp(ROOM_TIMELINE_INITIAL_PAGE_SIZE, ROOM_TIMELINE_PAGE_SIZE);
    handle.preferred_room_timeline_initial_page_size = clamped_page_size;
    Ok(())
}

pub fn paginate_active_room_timeline_backwards(
    handle_id: u64,
    page_size: u16,
) -> Result<(), String> {
    let (room_id, command_sender) = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get(&handle_id) else {
            return Err(format!("matrix-sdk backend runtime handle {handle_id} is not active"));
        };
        let Some(task) = handle.room_timeline_task.as_ref() else {
            return Err(format!(
                "matrix-sdk backend runtime handle {handle_id} has no active room timeline"
            ));
        };

        if task.thread.is_finished() {
            return Err(format!(
                "matrix-sdk active room timeline task for '{}' is no longer running",
                task.room_id
            ));
        }

        (task.room_id.clone(), task.commands.clone())
    };

    let page_size = if page_size == 0 {
        ROOM_TIMELINE_PAGE_SIZE
    } else {
        page_size
    };

    command_sender
        .send(MatrixBackendRoomTimelineCommand::PaginateBackwards(page_size))
        .map_err(|_| {
            format!(
                "failed to send pagination command to active matrix-sdk room timeline '{}'",
                room_id
            )
        })?;

    tracing::debug!(
        handle_id,
        room_id,
        page_size,
        "Queued matrix-sdk room timeline backwards pagination"
    );

    Ok(())
}

pub async fn fetch_active_room_timeline(handle_id: u64) -> Result<Vec<MatrixTimelineItem>, String> {
    let snapshot = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| {
            handle
                .room_timeline_snapshot
                .lock()
                .expect("poisoned matrix room timeline snapshot mutex")
                .clone()
        })
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;

    tracing::debug!(
        handle_id,
        item_count = snapshot.len(),
        "Fetched matrix room timeline snapshot"
    );

    Ok(snapshot)
}

pub async fn fetch_active_room_timeline_media_content(
    handle_id: u64,
    item_id: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    let client = client_for_handle(handle_id)?;
    let item_id = item_id.trim();
    if item_id.is_empty() {
        return Err("cannot fetch matrix-sdk timeline media without an item id".to_owned());
    }

    let media_request = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .and_then(|handle| {
            handle
                .room_timeline_media_lookup
                .lock()
                .expect("poisoned matrix room timeline media lookup mutex")
                .get(item_id)
                .cloned()
        })
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no active timeline media for item '{item_id}'"
            )
        })?;

    let request =
        build_timeline_media_request_parameters(&media_request, width, height, crop)?;

    tracing::debug!(
        handle_id,
        item_id,
        width,
        height,
        crop,
        "Fetching matrix-sdk active timeline media content"
    );

    client
        .media()
        .get_media_content(&request, false)
        .await
        .map_err(|e| format!("failed to fetch matrix-sdk active timeline media: {e}"))
}

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

    let mut config = AttachmentConfig::new();
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

pub async fn fetch_room_pinned_event_ids(
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<String>, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    load_cached_pinned_event_ids(&room).await
}

pub async fn pin_room_event(handle_id: u64, room_id: &str, event_id: &str) -> Result<(), String> {
    update_room_pinned_event_ids(handle_id, room_id, event_id, true).await
}

pub async fn unpin_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(), String> {
    update_room_pinned_event_ids(handle_id, room_id, event_id, false).await
}

pub async fn fetch_room_redaction_permissions(
    handle_id: u64,
    room_id: &str,
) -> Result<MatrixRoomRedactionPermissions, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let own_user_id = room.own_user_id().to_owned();
    let member = room
        .get_member(&own_user_id)
        .await
        .map_err(|e| format!("failed to fetch matrix-sdk room member permissions: {e}"))?
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} cannot resolve own member state in room {}",
                room_id.trim()
            )
        })?;

    Ok(MatrixRoomRedactionPermissions {
        can_redact_own: member.can_redact_own(),
        can_redact_other: member.can_redact_other(),
    })
}

pub struct RawEventDialogData {
    pub pretty_json: String,
    pub body: String,
    pub formatted_body: String,
}

/// Extracts the `type` and `content` JSON from a timeline event for forwarding.
///
/// Returns `(event_type, content_json)` — e.g. `("m.room.message", "{\"body\":...}")`.
/// The content JSON is the raw, unmodified event content from the server, preserving
/// all metadata fields (width, height, duration, thumbnail, blurhash, etc.).
pub async fn fetch_active_room_event_content_for_forwarding(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(String, String), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot extract event content without an event id".to_owned());
    }

    let timeline = room
        .timeline()
        .await
        .map_err(|e| format!("failed to build matrix-sdk room timeline for forwarding: {e}"))?;
    let items = timeline.items().await;

    for item in items.iter() {
        let Some(event) = item.as_event() else {
            continue;
        };
        let Some(current_event_id) = event.event_id() else {
            continue;
        };
        if current_event_id.as_str() != event_id {
            continue;
        }

        let raw_event = event.latest_json().ok_or_else(|| {
            format!("matrix-sdk room event '{event_id}' has no raw JSON available for forwarding")
        })?;

        let parsed: serde_json::Value =
            serde_json::from_str(raw_event.json().get()).map_err(|e| {
                format!("failed to parse raw JSON for matrix-sdk room event '{event_id}': {e}")
            })?;

        let event_type = parsed
            .get("type")
            .and_then(|v| v.as_str())
            .unwrap_or("m.room.message")
            .to_owned();

        let content = parsed
            .get("content")
            .ok_or_else(|| {
                format!("matrix-sdk room event '{event_id}' has no content field")
            })?;

        let content_json = serde_json::to_string(content).map_err(|e| {
            format!("failed to serialize content of matrix-sdk room event '{event_id}': {e}")
        })?;

        return Ok((event_type, content_json));
    }

    Err(format!(
        "matrix-sdk room timeline for '{}' does not currently include event '{event_id}'",
        room_id.trim(),
    ))
}

pub async fn fetch_active_room_raw_event_dialog_data(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<RawEventDialogData, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err(
            "cannot inspect a matrix-sdk room event without an event id".to_owned(),
        );
    }

    let timeline = room
        .timeline()
        .await
        .map_err(|e| format!("failed to build matrix-sdk room timeline for raw inspection: {e}"))?;
    let items = timeline.items().await;

    for item in items.iter() {
        let Some(event) = item.as_event() else {
            continue;
        };
        let Some(current_event_id) = event.event_id() else {
            continue;
        };
        if current_event_id.as_str() != event_id {
            continue;
        }

        let raw_event = event.latest_json().ok_or_else(|| {
            format!(
                "matrix-sdk room event '{event_id}' does not currently have raw JSON available"
            )
        })?;

        let raw_json_str = raw_event.json().get();
        let parsed: serde_json::Value = serde_json::from_str(raw_json_str).map_err(|e| {
            format!("failed to parse raw JSON for matrix-sdk room event '{event_id}': {e}")
        })?;

        let pretty_json = {
            let mut buf = Vec::new();
            let formatter = serde_json::ser::PrettyFormatter::with_indent(b"    ");
            let mut serializer = serde_json::Serializer::with_formatter(&mut buf, formatter);
            serde::Serialize::serialize(&parsed, &mut serializer)
                .ok()
                .and_then(|_| String::from_utf8(buf).ok())
                .unwrap_or_else(|| raw_json_str.to_owned())
        };

        let body = parsed
            .get("content")
            .and_then(|c| c.get("body"))
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_owned();

        let formatted_body = parsed
            .get("content")
            .and_then(|c| c.get("formatted_body"))
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_owned();

        return Ok(RawEventDialogData {
            pretty_json,
            body,
            formatted_body,
        });
    }

    Err(format!(
        "matrix-sdk room timeline for '{}' does not currently include event '{}'",
        room_id.trim(),
        event_id
    ))
}

pub async fn fetch_room_read_receipts(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<Vec<MatrixReadReceiptEntry>, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err(
            "cannot inspect matrix-sdk room read receipts without an event id".to_owned(),
        );
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;

    let mut receipts = room
        .load_event_receipts(ReceiptType::Read, ReceiptThread::Unthreaded, &parsed_event_id)
        .await
        .map_err(|e| format!("failed to load matrix-sdk room read receipts: {e}"))?;

    receipts.sort_by(|a, b| {
        let a_ts = a.1.ts.map(|ts| u64::from(ts.0)).unwrap_or(0);
        let b_ts = b.1.ts.map(|ts| u64::from(ts.0)).unwrap_or(0);
        b_ts.cmp(&a_ts).then_with(|| a.0.as_str().cmp(b.0.as_str()))
    });

    let mut entries = Vec::with_capacity(receipts.len());
    for (user_id, receipt) in receipts {
        let member = room
            .get_member(&user_id)
            .await
            .map_err(|e| format!("failed to fetch matrix-sdk room member for receipt: {e}"))?;
        let display_name = member
            .as_ref()
            .and_then(|member| member.display_name().map(ToOwned::to_owned))
            .unwrap_or_else(|| user_id.to_string());
        let avatar_url = member
            .as_ref()
            .and_then(|member| member.avatar_url().map(ToString::to_string))
            .map(normalize_mxc_uri)
            .unwrap_or_default();
        let timestamp = receipt.ts.map(|ts| u64::from(ts.0)).unwrap_or(0);

        entries.push(MatrixReadReceiptEntry {
            user_id: user_id.to_string(),
            display_name,
            avatar_url,
            timestamp,
        });
    }

    Ok(entries)
}

async fn update_room_pinned_event_ids(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    should_pin: bool,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot update matrix-sdk room pinned events without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;
    let mut pinned_event_ids = load_cached_pinned_event_ids(&room)
        .await?
        .into_iter()
        .map(|event_id| {
            EventId::parse(&event_id)
                .map_err(|e| format!("invalid pinned event id '{event_id}' from state store: {e}"))
        })
        .collect::<Result<Vec<_>, _>>()?;

    let mut changed = false;
    if should_pin {
        if !pinned_event_ids.iter().any(|candidate| candidate == &parsed_event_id) {
            pinned_event_ids.push(parsed_event_id.clone());
            changed = true;
        }
    } else {
        let original_len = pinned_event_ids.len();
        pinned_event_ids.retain(|candidate| candidate != &parsed_event_id);
        changed = pinned_event_ids.len() != original_len;
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        should_pin,
        changed,
        "Updating matrix-sdk room pinned events"
    );

    if !changed {
        return Ok(());
    }

    room.send_state_event(RoomPinnedEventsEventContent::new(pinned_event_ids))
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to update matrix-sdk room pinned events: {e}"))
}

async fn load_cached_pinned_event_ids(room: &Room) -> Result<Vec<String>, String> {
    let Some(raw_event) = room
        .get_state_event_static::<RoomPinnedEventsEventContent>()
        .await
        .map_err(|e| format!("failed to load matrix-sdk room pinned events from state store: {e}"))?
    else {
        return Ok(Vec::new());
    };

    let event = raw_event.deserialize().map_err(|e| {
        format!("failed to deserialize matrix-sdk room pinned events from state store: {e}")
    })?;

    Ok(match event {
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Sync(ev) => ev
            .as_original()
            .map(|ev| {
                ev.content
                    .pinned
                    .iter()
                    .map(|event_id| event_id.to_string())
                    .collect()
            })
            .unwrap_or_default(),
        matrix_sdk::deserialized_responses::SyncOrStrippedState::Stripped(_) => Vec::new(),
    })
}

async fn run_room_timeline_loop(
    handle_id: u64,
    generation: u64,
    generation_counter: Arc<AtomicU64>,
    client: Client,
    room_id: String,
    initial_page_size: u16,
    room_timeline_snapshot: Arc<Mutex<Vec<MatrixTimelineItem>>>,
    room_timeline_media_lookup: Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
    mut commands: mpsc::UnboundedReceiver<MatrixBackendRoomTimelineCommand>,
    stop_requested: Arc<AtomicBool>,
) {
    tracing::info!(handle_id, room_id, "Running matrix-sdk room timeline loop");
    let loop_started_at = Instant::now();

    let parsed_room_id = match RoomId::parse(&room_id) {
        Ok(room_id) => room_id,
        Err(error) => {
            tracing::warn!(handle_id, room_id, %error, "Invalid room id for room timeline task");
            return;
        }
    };

    let get_room_started_at = Instant::now();
    let Some(room) = client.get_room(&parsed_room_id) else {
        tracing::warn!(handle_id, room_id, "Matrix-sdk client does not know the requested room");
        return;
    };
    log_room_timeline_perf(
        handle_id,
        &room_id,
        "rust.matrix_timeline.get_room",
        get_room_started_at.elapsed(),
        "",
    );

    let build_started_at = Instant::now();
    let timeline = match room.timeline().await {
        Ok(timeline) => timeline,
        Err(error) => {
            tracing::warn!(handle_id, room_id, %error, "Failed to build matrix-sdk timeline");
            return;
        }
    };
    log_room_timeline_perf(
        handle_id,
        &room_id,
        "rust.matrix_timeline.timeline_build",
        build_started_at.elapsed(),
        "",
    );

    let own_user_id = client.user_id();

    let subscribe_started_at = Instant::now();
    let (items, stream) = timeline.subscribe().await;
    log_room_timeline_perf(
        handle_id,
        &room_id,
        "rust.matrix_timeline.subscribe",
        subscribe_started_at.elapsed(),
        &format!(" subscribe_count={}", items.len()),
    );
    let mut current_values = items;
    let subscribe_count = current_values.len();
    {
        let snapshot_build_started_at = Instant::now();
        let (snapshot, media_lookup) = build_room_timeline_snapshot(&current_values, own_user_id);
        let snapshot_build_elapsed = snapshot_build_started_at.elapsed();
        let snapshot_count = snapshot.len();
        // Lock the snapshot BEFORE checking the generation so that a
        // concurrent select_active_room_timeline (which bumps the generation
        // and then clears the snapshot under the same lock) cannot interleave
        // between our check and our write.  This eliminates the TOCTOU race
        // where a stale room's task could overwrite a newer room's snapshot.
        {
            let mut snapshot_guard = room_timeline_snapshot
                .lock()
                .expect("poisoned matrix room timeline snapshot mutex");
            if generation_counter.load(Ordering::Acquire) != generation {
                tracing::debug!(
                    handle_id,
                    room_id,
                    generation,
                    "Discarding stale initial matrix-sdk timeline snapshot for an inactive room generation"
                );
                return;
            }
            *snapshot_guard = snapshot;
        }
        {
            let mut media_guard = room_timeline_media_lookup
                .lock()
                .expect("poisoned matrix room timeline media lookup mutex");
            if generation_counter.load(Ordering::Acquire) != generation {
                return;
            }
            *media_guard = media_lookup;
        }
        tracing::info!(
            handle_id,
            room_id,
            subscribe_count,
            snapshot_count,
            "Initial matrix-sdk timeline subscribe"
        );
        log_room_timeline_perf(
            handle_id,
            &room_id,
            "rust.matrix_timeline.initial_snapshot_build",
            snapshot_build_elapsed,
            &format!(
                " subscribe_count={} snapshot_count={}",
                subscribe_count, snapshot_count
            ),
        );
        // Only notify C++ when there are actual items.  For rooms with
        // no cached events the initial subscribe returns 0 items; sending
        // an empty notification would clear the loading flag prematurely
        // (showing "Nothing has loaded" while pagination is still running).
        if snapshot_count > 0 {
            crate::ffi::matrix_notify_room_timeline_snapshot_updated(handle_id, &room_id);
            log_room_timeline_perf(
                handle_id,
                &room_id,
                "rust.matrix_timeline.initial_snapshot_notified",
                loop_started_at.elapsed(),
                &format!(
                    " subscribe_count={} snapshot_count={}",
                    subscribe_count, snapshot_count
                ),
            );
        }
    }

    tracing::info!(
        handle_id,
        room_id,
        initial_page_size,
        "Requesting initial backwards pagination"
    );
    let paginate_started_at = Instant::now();
    // Run the initial pagination concurrently with the stream loop so
    // diffs can be processed as they arrive instead of buffering until
    // the paginate future completes.
    let initial_paginate_fut = timeline.paginate_backwards(initial_page_size);
    tokio::pin!(initial_paginate_fut);
    let mut initial_paginate_pending = true;

    let mut stream = Box::pin(stream);
    let mut first_diff_logged = false;
    while !stop_requested.load(Ordering::Relaxed) {
        tokio::select! {
            result = &mut initial_paginate_fut, if initial_paginate_pending => {
                initial_paginate_pending = false;
                if let Err(error) = result {
                    tracing::warn!(
                        handle_id,
                        room_id,
                        %error,
                        "Initial matrix-sdk room timeline pagination failed"
                    );
                }
                log_room_timeline_perf(
                    handle_id,
                    &room_id,
                    "rust.matrix_timeline.initial_paginate",
                    paginate_started_at.elapsed(),
                    &format!(" page_size={}", initial_page_size),
                );
            }

            maybe_diffs = stream.next() => {
                match maybe_diffs {
                    Some(diffs) => {
                        let diffs: Vec<VectorDiff<Arc<TimelineItem>>> = diffs;
                        let first_diff_started_at = Instant::now();
                        for diff in diffs.iter().cloned() {
                            diff.apply(&mut current_values);
                        }

                        let snapshot_build_started_at = Instant::now();
                        let (snapshot, media_lookup) =
                            build_room_timeline_snapshot(&current_values, own_user_id);
                        let snapshot_build_elapsed = snapshot_build_started_at.elapsed();
                        let item_count = snapshot.len();
                        // Lock-then-check: hold the snapshot lock while
                        // verifying the generation to prevent a stale task
                        // from overwriting a newer room's data (TOCTOU fix).
                        {
                            let mut snapshot_guard = room_timeline_snapshot
                                .lock()
                                .expect("poisoned matrix room timeline snapshot mutex");
                            if generation_counter.load(Ordering::Acquire) != generation {
                                tracing::debug!(
                                    handle_id,
                                    room_id,
                                    generation,
                                    "Stopping stale matrix-sdk room timeline loop after room switch"
                                );
                                break;
                            }
                            *snapshot_guard = snapshot;
                        }
                        {
                            let mut media_guard = room_timeline_media_lookup
                                .lock()
                                .expect("poisoned matrix room timeline media lookup mutex");
                            if generation_counter.load(Ordering::Acquire) != generation {
                                break;
                            }
                            *media_guard = media_lookup;
                        }
                        crate::ffi::matrix_notify_room_timeline_snapshot_updated(handle_id, &room_id);

                        let diff_count = diffs.len();
                        tracing::info!(
                            handle_id,
                            room_id,
                            item_count,
                            diff_count,
                            "Updated matrix-sdk room timeline snapshot"
                        );
                        if !first_diff_logged {
                            first_diff_logged = true;
                            log_room_timeline_perf(
                                handle_id,
                                &room_id,
                                "rust.matrix_timeline.first_diff_applied",
                                first_diff_started_at.elapsed(),
                                &format!(
                                    " diff_count={} current_value_count={} snapshot_count={}",
                                    diff_count,
                                    current_values.len(),
                                    item_count
                                ),
                            );
                            log_room_timeline_perf(
                                handle_id,
                                &room_id,
                                "rust.matrix_timeline.first_stream_snapshot_build",
                                snapshot_build_elapsed,
                                &format!(
                                    " diff_count={} current_value_count={} snapshot_count={}",
                                    diff_count,
                                    current_values.len(),
                                    item_count
                                ),
                            );
                            log_room_timeline_perf(
                                handle_id,
                                &room_id,
                                "rust.matrix_timeline.first_stream_snapshot_notified",
                                loop_started_at.elapsed(),
                                &format!(
                                    " diff_count={} current_value_count={} snapshot_count={}",
                                    diff_count,
                                    current_values.len(),
                                    item_count
                                ),
                            );

                            // Backfill the room-list entry if it still lacks a
                            // timestamp (sliding sync didn't deliver a latest
                            // event for this room).
                            {
                                let snap = room_timeline_snapshot
                                    .lock()
                                    .expect("poisoned matrix room timeline snapshot mutex");
                                maybe_backfill_room_list_preview(handle_id, &room_id, &snap);
                            }
                        }
                    }
                    None => {
                        tracing::info!(handle_id, room_id, "Matrix-sdk room timeline stream ended");
                        break;
                    }
                }
            }

            maybe_command = commands.recv() => {
                match maybe_command {
                    Some(MatrixBackendRoomTimelineCommand::PaginateBackwards(page_size)) => {
                        tracing::info!(
                            handle_id,
                            room_id,
                            page_size,
                            "Paginating active matrix-sdk room timeline backwards"
                        );
                        if let Err(error) = timeline.paginate_backwards(page_size).await {
                            tracing::warn!(
                                handle_id,
                                room_id,
                                page_size,
                                %error,
                                "Failed to paginate active matrix-sdk room timeline backwards"
                            );
                            // Re-notify so the C++ side processes the current snapshot
                            // instead of waiting for a larger one that will never arrive.
                            crate::ffi::matrix_notify_room_timeline_snapshot_updated(
                                handle_id,
                                &room_id,
                            );
                        }
                    }
                    None => {
                        tracing::debug!(
                            handle_id,
                            room_id,
                            "Matrix-sdk room timeline command channel closed"
                        );
                        break;
                    }
                }
            }

            _ = tokio::time::sleep(Duration::from_millis(ROOM_TIMELINE_STOP_POLL_INTERVAL_MS)) => {
                if stop_requested.load(Ordering::Relaxed) {
                    break;
                }
            }
        }
    }

    tracing::info!(handle_id, room_id, "Matrix-sdk room timeline loop stopped");
}

fn build_room_timeline_snapshot(
    values: &Vector<Arc<TimelineItem>>,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
) -> (Vec<MatrixTimelineItem>, HashMap<String, MatrixTimelineMediaRequest>) {
    let mut items = Vec::new();
    let mut media_lookup = HashMap::new();

    for item in values.iter() {
        if let Some((summary, media_request)) =
            timeline_item_to_summary(item.as_ref(), own_user_id)
        {
            if let Some(media_request) = media_request {
                media_lookup.insert(summary.item_id.clone(), media_request.clone());
                if !summary.event_id.is_empty() {
                    media_lookup.insert(summary.event_id.clone(), media_request);
                }
            }
            items.push(summary);
        }
    }

    // Reverse so index 0 = newest, matching the BottomToTop ListView
    // layout in QML where index 0 sits at the visual bottom.
    items.reverse();

    (items, media_lookup)
}

fn timeline_item_to_summary(
    item: &TimelineItem,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
) -> Option<(MatrixTimelineItem, Option<MatrixTimelineMediaRequest>)> {
    let item_id = item.unique_id().0.clone();

    if let Some(event) = item.as_event() {
        let sender_id = event.sender().to_string();
        let (sender_display_name, sender_avatar_url) = match event.sender_profile() {
            TimelineDetails::Ready(profile) => (
                profile
                    .display_name
                    .clone()
                    .unwrap_or_else(|| sender_id.clone()),
                profile
                    .avatar_url
                    .as_ref()
                    .map(|url| normalize_mxc_uri(url.to_string()))
                    .unwrap_or_default(),
            ),
            _ => (sender_id.clone(), String::new()),
        };
        let summary = summarize_timeline_content(event.content(), own_user_id);
        let body = summary.body;
        let formatted_body = summary.formatted_body;
        let thread_id = summary.thread_root_id;
        let reply_event_id = summary.reply_event_id;
        let reply_sender_id = summary.reply_sender_id;
        let reply_sender_display_name = summary.reply_sender_display_name;
        let reply_item_kind = summary.reply_item_kind;
        let reply_matrix_event_type = summary.reply_matrix_event_type;
        let reply_body = summary.reply_body;
        let reply_formatted_body = summary.reply_formatted_body;
        let reply_media = summary.reply_media;
        let reactions = summary.reactions;
        let reactions_summary = summary.reactions_summary;
        let special_effect_names = summary.special_effect_names;
        let item_kind = summary.kind;
        let matrix_event_type = summary.matrix_event_type;
        let is_edited = summary.is_edited;
        let media = summary.media;
        let media_request = media.as_ref().and_then(|media| {
            media.source.clone().map(|source| MatrixTimelineMediaRequest {
                source,
                thumbnail_source: media.thumbnail_source.clone(),
            })
        });

        return Some((
            MatrixTimelineItem {
                item_id,
                event_id: event.event_id().map(ToString::to_string).unwrap_or_default(),
                delivery_state: matrix_timeline_delivery_state(event),
                thread_id,
                sender_id,
                sender_display_name,
                sender_avatar_url,
                body,
                formatted_body,
                reply_event_id,
                reply_sender_id,
                reply_sender_display_name,
                reply_item_kind,
                reply_matrix_event_type,
                reply_body,
                reply_formatted_body,
                reply_media_url: reply_media
                    .as_ref()
                    .map(|media| media.media_url.clone())
                    .unwrap_or_default(),
                reply_thumbnail_url: reply_media
                    .as_ref()
                    .map(|media| media.thumbnail_url.clone())
                    .unwrap_or_default(),
                reply_file_name: reply_media
                    .as_ref()
                    .map(|media| media.file_name.clone())
                    .unwrap_or_default(),
                reply_mime_type: reply_media
                    .as_ref()
                    .map(|media| media.mime_type.clone())
                    .unwrap_or_default(),
                reply_media_width: reply_media
                    .as_ref()
                    .map(|media| media.media_width)
                    .unwrap_or(0),
                reply_media_height: reply_media
                    .as_ref()
                    .map(|media| media.media_height)
                    .unwrap_or(0),
                reply_media_duration_ms: reply_media
                    .as_ref()
                    .map(|media| media.media_duration_ms)
                    .unwrap_or(0),
                reply_media_size_bytes: reply_media
                    .as_ref()
                    .map(|media| media.media_size_bytes)
                    .unwrap_or(0),
                reactions,
                reactions_summary,
                special_effect_names,
                item_kind,
                matrix_event_type,
                is_edited,
                media_url: media
                    .as_ref()
                    .map(|media| media.media_url.clone())
                    .unwrap_or_default(),
                thumbnail_url: media
                    .as_ref()
                    .map(|media| media.thumbnail_url.clone())
                    .unwrap_or_default(),
                file_name: media
                    .as_ref()
                    .map(|media| media.file_name.clone())
                    .unwrap_or_default(),
                mime_type: media
                    .as_ref()
                    .map(|media| media.mime_type.clone())
                    .unwrap_or_default(),
                media_width: media.as_ref().map(|media| media.media_width).unwrap_or(0),
                media_height: media
                    .as_ref()
                    .map(|media| media.media_height)
                    .unwrap_or(0),
                media_duration_ms: media
                    .as_ref()
                    .map(|media| media.media_duration_ms)
                    .unwrap_or(0),
                media_size_bytes: media
                    .as_ref()
                    .map(|media| media.media_size_bytes)
                    .unwrap_or(0),
                media_is_encrypted: media
                    .as_ref()
                    .map(|media| media.media_is_encrypted)
                    .unwrap_or(false),
                thumbnail_is_encrypted: media
                    .as_ref()
                    .map(|media| media.thumbnail_is_encrypted)
                    .unwrap_or(false),
                timestamp: u64::from(event.timestamp().get()),
                is_own: event.is_own(),
            },
            media_request,
        ));
    }

    match item.as_virtual() {
        Some(VirtualTimelineItem::DateDivider(timestamp)) => Some((
            MatrixTimelineItem {
                item_id,
                event_id: String::new(),
                delivery_state: String::new(),
                thread_id: String::new(),
                sender_id: String::new(),
                sender_display_name: String::new(),
                sender_avatar_url: String::new(),
                body: String::new(),
                formatted_body: String::new(),
                reply_event_id: String::new(),
                reply_sender_id: String::new(),
                reply_sender_display_name: String::new(),
                reply_item_kind: String::new(),
                reply_matrix_event_type: String::new(),
                reply_body: String::new(),
                reply_formatted_body: String::new(),
                reply_media_url: String::new(),
                reply_thumbnail_url: String::new(),
                reply_file_name: String::new(),
                reply_mime_type: String::new(),
                reply_media_width: 0,
                reply_media_height: 0,
                reply_media_duration_ms: 0,
                reply_media_size_bytes: 0,
                reactions: Vec::new(),
                reactions_summary: String::new(),
                special_effect_names: Vec::new(),
                item_kind: "date_divider".to_owned(),
                matrix_event_type: String::new(),
                is_edited: false,
                media_url: String::new(),
                thumbnail_url: String::new(),
                file_name: String::new(),
                mime_type: String::new(),
                media_width: 0,
                media_height: 0,
                media_duration_ms: 0,
                media_size_bytes: 0,
                media_is_encrypted: false,
                thumbnail_is_encrypted: false,
                timestamp: u64::from(timestamp.get()),
                is_own: false,
            },
            None,
        )),
        Some(VirtualTimelineItem::ReadMarker) | Some(VirtualTimelineItem::TimelineStart) | None => {
            None
        }
    }
}

fn matrix_timeline_delivery_state(event: &matrix_sdk_ui::timeline::EventTimelineItem) -> String {
    use matrix_sdk_ui::timeline::EventSendState;

    match event.send_state() {
        Some(EventSendState::NotSentYet { .. }) => "pending".to_owned(),
        Some(EventSendState::Sent { .. }) => "sent".to_owned(),
        Some(EventSendState::SendingFailed { .. }) => "failed".to_owned(),
        None => String::new(),
    }
}

fn build_timeline_media_request_parameters(
    media_request: &MatrixTimelineMediaRequest,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<MediaRequestParameters, String> {
    if width > 0 && height > 0 {
        if let Some(thumbnail_source) = media_request.thumbnail_source.clone() {
            return Ok(MediaRequestParameters {
                source: thumbnail_source,
                format: MediaFormat::File,
            });
        }

        if matches!(&media_request.source, MediaSource::Plain(_)) {
            let width =
                UInt::try_from(width).map_err(|_| format!("invalid thumbnail width: {width}"))?;
            let height =
                UInt::try_from(height).map_err(|_| format!("invalid thumbnail height: {height}"))?;
            let method = if crop { Method::Crop } else { Method::Scale };

            return Ok(MediaRequestParameters {
                source: media_request.source.clone(),
                format: MediaFormat::Thumbnail(MediaThumbnailSettings::with_method(
                    method, width, height,
                )),
            });
        }
    }

    Ok(MediaRequestParameters {
        source: media_request.source.clone(),
        format: MediaFormat::File,
    })
}
