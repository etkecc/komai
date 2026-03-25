// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use matrix_sdk::attachment::AttachmentConfig;
use mime::Mime;
use std::{fs, path::Path};

pub fn select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<(), String> {
    let room_id = room_id.trim();

    let (client, room_timeline_snapshot, previous_task) = {
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
        handle
            .room_timeline_snapshot
            .lock()
            .expect("poisoned matrix room timeline snapshot mutex")
            .clear();

        (
            handle.client.clone(),
            Arc::clone(&handle.room_timeline_snapshot),
            previous_task,
        )
    };

    if let Some(previous_task) = previous_task {
        stop_room_timeline_task(handle_id, previous_task);
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
    let room_timeline_task = std::thread::spawn(move || {
        crate::runtime().block_on(run_room_timeline_loop(
            handle_id,
            client,
            room_id_for_thread,
            room_timeline_snapshot,
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

pub async fn send_room_message(
    handle_id: u64,
    room_id: &str,
    body: &str,
    formatted_html: &str,
    message_kind: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let room_id = room_id.trim();
    if room_id.is_empty() {
        return Err("cannot send a matrix-sdk room message without a room id".to_owned());
    }

    let body = body.trim();
    if body.is_empty() {
        return Err("cannot send an empty matrix-sdk room message".to_owned());
    }

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

    let formatted_html = formatted_html.trim();
    let content: AnyMessageLikeEventContent = match message_kind {
        "text" => {
            if formatted_html.is_empty() {
                RoomMessageEventContent::text_plain(body).into()
            } else {
                RoomMessageEventContent::text_html(body, formatted_html).into()
            }
        }
        "notice" => {
            if formatted_html.is_empty() {
                RoomMessageEventContent::notice_plain(body).into()
            } else {
                RoomMessageEventContent::notice_html(body, formatted_html).into()
            }
        }
        "emote" => {
            if formatted_html.is_empty() {
                RoomMessageEventContent::emote_plain(body).into()
            } else {
                RoomMessageEventContent::emote_html(body, formatted_html).into()
            }
        }
        other => {
            return Err(format!(
                "unsupported matrix-sdk room message kind '{other}' for room '{room_id}'"
            ));
        }
    };

    tracing::info!(
        handle_id,
        room_id,
        message_kind,
        has_formatted_html = !formatted_html.is_empty(),
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

pub async fn send_room_attachment(
    handle_id: u64,
    room_id: &str,
    file_path: &str,
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
    let filename = Path::new(file_path)
        .file_name()
        .and_then(|name| name.to_str())
        .filter(|name| !name.is_empty())
        .ok_or_else(|| format!("attachment path '{file_path}' does not include a file name"))?
        .to_owned();

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        file_path,
        mime_type,
        file_size = data.len(),
        "Sending matrix-sdk room attachment"
    );

    room.send_attachment(filename, &mime, data, AttachmentConfig::new())
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to send matrix-sdk room attachment: {e}"))
}

async fn run_room_timeline_loop(
    handle_id: u64,
    client: Client,
    room_id: String,
    room_timeline_snapshot: Arc<Mutex<Vec<MatrixTimelineItem>>>,
    mut commands: mpsc::UnboundedReceiver<MatrixBackendRoomTimelineCommand>,
    stop_requested: Arc<AtomicBool>,
) {
    tracing::info!(handle_id, room_id, "Running matrix-sdk room timeline loop");

    let parsed_room_id = match RoomId::parse(&room_id) {
        Ok(room_id) => room_id,
        Err(error) => {
            tracing::warn!(handle_id, room_id, %error, "Invalid room id for room timeline task");
            return;
        }
    };

    let Some(room) = client.get_room(&parsed_room_id) else {
        tracing::warn!(handle_id, room_id, "Matrix-sdk client does not know the requested room");
        return;
    };

    let timeline = match room.timeline().await {
        Ok(timeline) => timeline,
        Err(error) => {
            tracing::warn!(handle_id, room_id, %error, "Failed to build matrix-sdk timeline");
            return;
        }
    };

    if let Err(error) = timeline.paginate_backwards(ROOM_TIMELINE_PAGE_SIZE).await {
        tracing::debug!(
            handle_id,
            room_id,
            %error,
            "Initial matrix-sdk room timeline pagination failed"
        );
    }

    let (items, stream) = timeline.subscribe().await;
    let mut current_values = items;
    {
        let snapshot = build_room_timeline_snapshot(&current_values);
        *room_timeline_snapshot
            .lock()
            .expect("poisoned matrix room timeline snapshot mutex") = snapshot;
        crate::ffi::matrix_notify_room_timeline_snapshot_updated(handle_id, &room_id);
    }

    let mut stream = Box::pin(stream);
    while !stop_requested.load(Ordering::Relaxed) {
        tokio::select! {
            maybe_diffs = stream.next() => {
                match maybe_diffs {
                    Some(diffs) => {
                        let diffs: Vec<VectorDiff<Arc<TimelineItem>>> = diffs;
                        for diff in diffs.iter().cloned() {
                            diff.apply(&mut current_values);
                        }

                        let snapshot = build_room_timeline_snapshot(&current_values);
                        let item_count = snapshot.len();
                        *room_timeline_snapshot
                            .lock()
                            .expect("poisoned matrix room timeline snapshot mutex") = snapshot;
                        crate::ffi::matrix_notify_room_timeline_snapshot_updated(handle_id, &room_id);

                        tracing::debug!(
                            handle_id,
                            room_id,
                            item_count,
                            "Updated matrix-sdk room timeline snapshot"
                        );
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
                        tracing::debug!(
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

            _ = tokio::time::sleep(Duration::from_millis(200)) => {
                if stop_requested.load(Ordering::Relaxed) {
                    break;
                }
            }
        }
    }

    tracing::info!(handle_id, room_id, "Matrix-sdk room timeline loop stopped");
}

fn build_room_timeline_snapshot(values: &Vector<Arc<TimelineItem>>) -> Vec<MatrixTimelineItem> {
    values
        .iter()
        .filter_map(|item| timeline_item_to_summary(item.as_ref()))
        .collect()
}

fn timeline_item_to_summary(item: &TimelineItem) -> Option<MatrixTimelineItem> {
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
        let (item_kind, body) = timeline_event_content_summary(event.content());

        return Some(MatrixTimelineItem {
            item_id,
            event_id: event.event_id().map(ToString::to_string).unwrap_or_default(),
            sender_id,
            sender_display_name,
            sender_avatar_url,
            body,
            item_kind,
            timestamp: u64::from(event.timestamp().get()),
            is_own: event.is_own(),
        });
    }

    match item.as_virtual() {
        Some(VirtualTimelineItem::DateDivider(timestamp)) => Some(MatrixTimelineItem {
            item_id,
            event_id: String::new(),
            sender_id: String::new(),
            sender_display_name: String::new(),
            sender_avatar_url: String::new(),
            body: String::new(),
            item_kind: "date_divider".to_owned(),
            timestamp: u64::from(timestamp.get()),
            is_own: false,
        }),
        Some(VirtualTimelineItem::ReadMarker) | Some(VirtualTimelineItem::TimelineStart) | None => {
            None
        }
    }
}

fn timeline_event_content_summary(content: &TimelineItemContent) -> (String, String) {
    match content {
        TimelineItemContent::MsgLike(content) => match &content.kind {
            MsgLikeKind::Message(message) => match message.msgtype() {
                MessageType::Text(_) => ("message".to_owned(), message.body().to_owned()),
                MessageType::Notice(_) => ("notice".to_owned(), message.body().to_owned()),
                MessageType::Emote(_) => ("emote".to_owned(), message.body().to_owned()),
                MessageType::Image(_) => ("image".to_owned(), message.body().to_owned()),
                MessageType::Video(_) => ("video".to_owned(), message.body().to_owned()),
                MessageType::Audio(_) => ("audio".to_owned(), message.body().to_owned()),
                MessageType::File(_) => ("file".to_owned(), message.body().to_owned()),
                MessageType::Location(_) => ("location".to_owned(), message.body().to_owned()),
                _ => ("message".to_owned(), message.body().to_owned()),
            },
            MsgLikeKind::Sticker(_) => ("sticker".to_owned(), "[Sticker]".to_owned()),
            MsgLikeKind::Poll(_) => ("poll".to_owned(), "[Poll]".to_owned()),
            MsgLikeKind::Redacted => ("redacted".to_owned(), "[Redacted message]".to_owned()),
            MsgLikeKind::UnableToDecrypt(_) => (
                "unable_to_decrypt".to_owned(),
                "[Unable to decrypt message]".to_owned(),
            ),
            MsgLikeKind::Other(_) => (
                "other_message".to_owned(),
                "[Unsupported message event]".to_owned(),
            ),
        },
        TimelineItemContent::MembershipChange(_) => (
            "membership_change".to_owned(),
            "[Membership change]".to_owned(),
        ),
        TimelineItemContent::ProfileChange(_) => {
            ("profile_change".to_owned(), "[Profile change]".to_owned())
        }
        TimelineItemContent::OtherState(_) => ("other_state".to_owned(), "[State event]".to_owned()),
        TimelineItemContent::FailedToParseMessageLike { .. } => (
            "failed_to_parse_message_like".to_owned(),
            "[Unreadable message event]".to_owned(),
        ),
        TimelineItemContent::FailedToParseState { .. } => (
            "failed_to_parse_state".to_owned(),
            "[Unreadable state event]".to_owned(),
        ),
        TimelineItemContent::CallInvite => ("call_invite".to_owned(), "[Call invite]".to_owned()),
        TimelineItemContent::RtcNotification => (
            "rtc_notification".to_owned(),
            "[RTC notification]".to_owned(),
        ),
    }
}
