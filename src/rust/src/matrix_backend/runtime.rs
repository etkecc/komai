// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    collections::HashMap,
    sync::{
        Arc, Mutex, OnceLock,
        atomic::{AtomicBool, AtomicU64, Ordering},
    },
    thread::JoinHandle,
    time::Duration,
};

use matrix_sdk::{
    Client,
    media::{MediaFormat, MediaRequestParameters, MediaThumbnailSettings},
    RoomState,
    ruma::{
        MxcUri, UInt,
        RoomId,
        api::client::media::get_content_thumbnail::v3::Method,
        api::client::profile::{AvatarUrl, DisplayName},
        events::{
            AnyMessageLikeEventContent,
            room::{
                MediaSource,
                message::{MessageType, RoomMessageEventContent},
            },
        },
    },
    stream::StreamExt,
};
use matrix_sdk_ui::{
    RoomListService,
    eyeball_im::{Vector, VectorDiff},
    room_list_service::{RoomListItem, filters},
    timeline::{
        MsgLikeKind, RoomExt, TimelineDetails, TimelineItem, TimelineItemContent,
        VirtualTimelineItem,
    },
};
use tokio::sync::mpsc;

use super::bootstrap;

pub struct MatrixBackendHandleInfo {
    pub handle_id: u64,
    pub has_session: bool,
    pub homeserver_url: String,
    pub user_id: String,
    pub device_id: String,
}

pub struct MatrixOwnProfile {
    pub display_name: String,
    pub avatar_url: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixRoomSummary {
    pub room_id: String,
    pub display_name: String,
    pub avatar_url: String,
    pub topic: String,
    pub direct_chat_other_user_id: String,
    pub is_invite: bool,
    pub is_space: bool,
    pub is_direct: bool,
    pub is_bot_room: bool,
    pub is_encrypted: bool,
    pub unread_message_count: u64,
    pub notification_count: u64,
    pub highlight_count: u64,
    pub timestamp: u64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixTimelineItem {
    pub item_id: String,
    pub event_id: String,
    pub sender_id: String,
    pub sender_display_name: String,
    pub sender_avatar_url: String,
    pub body: String,
    pub item_kind: String,
    pub timestamp: u64,
    pub is_own: bool,
}

static NEXT_BACKEND_HANDLE_ID: AtomicU64 = AtomicU64::new(1);
const ROOM_LIST_PAGE_SIZE: usize = 100_000;
const ROOM_TIMELINE_PAGE_SIZE: u16 = 50;

struct MatrixBackendHandle {
    client: Client,
    sync_task: Option<MatrixBackendSyncTask>,
    room_list_snapshot: Arc<Mutex<Vec<MatrixRoomSummary>>>,
    room_timeline_task: Option<MatrixBackendRoomTimelineTask>,
    room_timeline_snapshot: Arc<Mutex<Vec<MatrixTimelineItem>>>,
}

struct MatrixBackendSyncTask {
    stop_requested: Arc<AtomicBool>,
    thread: JoinHandle<()>,
}

struct MatrixBackendRoomTimelineTask {
    room_id: String,
    commands: mpsc::UnboundedSender<MatrixBackendRoomTimelineCommand>,
    stop_requested: Arc<AtomicBool>,
    thread: JoinHandle<()>,
}

enum MatrixBackendRoomTimelineCommand {
    PaginateBackwards(u16),
}

struct MatrixRoomClassification {
    direct_chat_other_user_id: String,
    is_direct: bool,
    is_bot_room: bool,
}

fn backend_handles() -> &'static Mutex<HashMap<u64, MatrixBackendHandle>> {
    static HANDLES: OnceLock<Mutex<HashMap<u64, MatrixBackendHandle>>> = OnceLock::new();
    HANDLES.get_or_init(|| Mutex::new(HashMap::new()))
}

fn client_for_handle(handle_id: u64) -> Result<Client, String> {
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| handle.client.clone())
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))
}

pub async fn start_restored_backend(profile_id: &str) -> Result<MatrixBackendHandleInfo, String> {
    tracing::info!(profile_id, "Starting restored matrix-sdk backend runtime");

    let Some(restored) = bootstrap::restore_client(profile_id).await? else {
        tracing::info!(profile_id, "No persisted matrix-sdk session is available for restore");
        return Ok(MatrixBackendHandleInfo {
            handle_id: 0,
            has_session: false,
            homeserver_url: String::new(),
            user_id: String::new(),
            device_id: String::new(),
        });
    };

    let handle_id = NEXT_BACKEND_HANDLE_ID.fetch_add(1, Ordering::Relaxed);
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .insert(
            handle_id,
            MatrixBackendHandle {
                client: restored.client,
                sync_task: None,
                room_list_snapshot: Arc::new(Mutex::new(Vec::new())),
                room_timeline_task: None,
                room_timeline_snapshot: Arc::new(Mutex::new(Vec::new())),
            },
        );

    tracing::info!(
        profile_id,
        handle_id,
        user_id = %restored.user_id,
        device_id = %restored.device_id,
        homeserver_url = %restored.homeserver_url,
        "Started restored matrix-sdk backend runtime"
    );

    Ok(MatrixBackendHandleInfo {
        handle_id,
        has_session: true,
        homeserver_url: restored.homeserver_url,
        user_id: restored.user_id,
        device_id: restored.device_id,
    })
}

pub fn stop_backend(handle_id: u64) -> Result<(), String> {
    if handle_id == 0 {
        return Ok(());
    }

    let removed = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .remove(&handle_id);

    if let Some(mut handle) = removed {
        if let Some(sync_task) = handle.sync_task.take() {
            stop_sync_task(handle_id, sync_task);
        }
        if let Some(room_timeline_task) = handle.room_timeline_task.take() {
            stop_room_timeline_task(handle_id, room_timeline_task);
        }
        tracing::info!(handle_id, "Stopped matrix-sdk backend runtime");
    } else {
        tracing::debug!(handle_id, "Matrix-sdk backend runtime handle was already absent");
    }
    Ok(())
}

fn stop_sync_task(handle_id: u64, sync_task: MatrixBackendSyncTask) {
    tracing::info!(handle_id, "Stopping matrix-sdk sync task");
    sync_task.stop_requested.store(true, Ordering::Relaxed);
    let _ = sync_task.thread.join();
}

fn stop_room_timeline_task(handle_id: u64, room_timeline_task: MatrixBackendRoomTimelineTask) {
    tracing::info!(
        handle_id,
        room_id = %room_timeline_task.room_id,
        "Stopping matrix-sdk room timeline task"
    );
    room_timeline_task
        .stop_requested
        .store(true, Ordering::Relaxed);
    let _ = room_timeline_task.thread.join();
}

pub fn start_sync(handle_id: u64) -> Result<(), String> {
    let (client, room_list_snapshot) = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get_mut(&handle_id) else {
            return Err(format!("matrix-sdk backend runtime handle {handle_id} is not active"));
        };

        if let Some(sync_task) = handle.sync_task.as_ref() {
            if !sync_task.thread.is_finished() {
                tracing::debug!(handle_id, "Matrix-sdk sync task is already running");
                return Ok(());
            }
        }

        (handle.client.clone(), Arc::clone(&handle.room_list_snapshot))
    };

    let stop_requested = Arc::new(AtomicBool::new(false));
    let stop_requested_for_thread = Arc::clone(&stop_requested);
    let sync_task = std::thread::spawn(move || {
        crate::runtime().block_on(run_sync_loop(
            handle_id,
            client,
            room_list_snapshot,
            stop_requested_for_thread,
        ));
    });

    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .entry(handle_id)
        .and_modify(|handle| {
            handle.sync_task = Some(MatrixBackendSyncTask {
                stop_requested,
                thread: sync_task,
            });
        });

    tracing::info!(handle_id, "Started matrix-sdk sync task");
    Ok(())
}

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

async fn run_sync_loop(
    handle_id: u64,
    client: Client,
    room_list_snapshot: Arc<Mutex<Vec<MatrixRoomSummary>>>,
    stop_requested: Arc<AtomicBool>,
) {
    tracing::info!(handle_id, "Running matrix-sdk room-list sync loop");

    let room_list_service = match RoomListService::new(client.clone()).await {
        Ok(service) => Arc::new(service),
        Err(error) => {
            tracing::warn!(handle_id, %error, "Failed to create matrix-sdk-ui RoomListService");
            return;
        }
    };

    let room_list = match room_list_service.all_rooms().await {
        Ok(room_list) => room_list,
        Err(error) => {
            tracing::warn!(handle_id, %error, "Failed to acquire matrix-sdk-ui room list");
            return;
        }
    };

    let (entries_stream, entries_controller) =
        room_list.entries_with_dynamic_adapters_with(ROOM_LIST_PAGE_SIZE, true);
    if !entries_controller.set_filter(Box::new(filters::new_filter_non_left())) {
        tracing::warn!(handle_id, "Failed to install matrix-sdk-ui room-list filter");
    }

    let sync_stream = room_list_service.sync();
    let mut entries_stream = Box::pin(entries_stream);
    let mut sync_stream = Box::pin(sync_stream);

    let mut current_values = Vector::<RoomListItem>::new();

    while !stop_requested.load(Ordering::Relaxed) {
        tokio::select! {
            maybe_diffs = entries_stream.next() => {
                match maybe_diffs {
                    Some(diffs) => {
                        let diffs: Vec<VectorDiff<RoomListItem>> = diffs;
                        for diff in diffs.iter().cloned() {
                            diff.apply(&mut current_values);
                        }

                        let snapshot = build_room_list_snapshot(&current_values);
                        let room_count = snapshot.len();
                        *room_list_snapshot
                            .lock()
                            .expect("poisoned matrix room-list snapshot mutex") = snapshot;

                        tracing::debug!(
                            handle_id,
                            room_count,
                            "Updated matrix-sdk room-list snapshot"
                        );
                    }
                    None => {
                        tracing::info!(handle_id, "Matrix-sdk-ui room-list entries stream ended");
                        break;
                    }
                }
            }

            maybe_sync = sync_stream.next() => {
                match maybe_sync {
                    Some(Ok(())) => {
                        tracing::debug!(handle_id, "Completed matrix-sdk-ui room-list sync iteration");
                    }
                    Some(Err(error)) => {
                        tracing::warn!(handle_id, %error, "Matrix-sdk-ui room-list sync failed");
                        break;
                    }
                    None => {
                        tracing::info!(handle_id, "Matrix-sdk-ui room-list sync stream ended");
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

    if let Err(error) = room_list_service.stop_sync() {
        tracing::debug!(handle_id, %error, "Stopping matrix-sdk-ui room-list sync returned an error");
    }

    tracing::info!(handle_id, "Matrix-sdk room-list sync loop stopped");
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

fn build_room_list_snapshot(values: &Vector<RoomListItem>) -> Vec<MatrixRoomSummary> {
    values.iter().map(room_list_item_to_summary).collect()
}

fn build_room_timeline_snapshot(
    values: &Vector<Arc<TimelineItem>>,
) -> Vec<MatrixTimelineItem> {
    values
        .iter()
        .filter_map(|item| timeline_item_to_summary(item.as_ref()))
        .collect()
}

fn normalize_mxc_uri(uri: String) -> String {
    if uri.is_empty() || uri.contains("://") {
        return uri;
    }

    format!("mxc://{uri}")
}

fn ci_contains(haystack: &str, needle: &str) -> bool {
    haystack.to_ascii_lowercase().contains(&needle.to_ascii_lowercase())
}

fn ci_starts_with(s: &str, prefix: &str) -> bool {
    s.get(..prefix.len())
        .is_some_and(|candidate| candidate.eq_ignore_ascii_case(prefix))
}

fn ci_ends_with(s: &str, suffix: &str) -> bool {
    if suffix.len() > s.len() {
        return false;
    }

    s.get(s.len() - suffix.len()..)
        .is_some_and(|candidate| candidate.eq_ignore_ascii_case(suffix))
}

fn is_likely_bot_user(user_id: &str, display_name: &str) -> bool {
    if ci_starts_with(user_id, "@bot") {
        return true;
    }

    if ci_contains(user_id, "bot:") {
        return true;
    }

    let localpart = user_id
        .split_once(':')
        .map(|(localpart, _)| localpart)
        .unwrap_or(user_id);
    if ci_contains(localpart, "puppet") {
        return false;
    }

    if ci_starts_with(user_id, "@_") {
        return true;
    }

    if ci_ends_with(localpart, "bridge") {
        return true;
    }

    if ci_contains(display_name, "bridge bot") {
        return true;
    }

    if ci_ends_with(display_name, "bot")
        && display_name
            .chars()
            .rev()
            .nth(3)
            .is_none_or(|c| !c.is_ascii_alphabetic())
    {
        return true;
    }

    if ci_starts_with(display_name, "bot")
        && display_name
            .chars()
            .nth(3)
            .is_none_or(|c| !c.is_ascii_alphabetic())
    {
        return true;
    }

    false
}

fn room_hero_candidates(room: &RoomListItem) -> Vec<(String, String)> {
    let own_user_id = room.own_user_id();
    let mut candidates: Vec<(String, String)> = room
        .heroes()
        .into_iter()
        .filter(|hero| hero.user_id != own_user_id)
        .map(|hero| (hero.user_id.to_string(), hero.display_name.unwrap_or_default()))
        .collect();

    candidates.sort_by(|left, right| left.0.cmp(&right.0));
    candidates.dedup_by(|left, right| left.0 == right.0);
    candidates
}

fn classify_room(room: &RoomListItem) -> MatrixRoomClassification {
    let hero_candidates = room_hero_candidates(room);

    let mut direct_targets: Vec<String> = room
        .direct_targets()
        .into_iter()
        .filter_map(|target| target.into_user_id())
        .map(|user_id| user_id.to_string())
        .collect();
    direct_targets.sort();
    direct_targets.dedup();

    if let Some(partner_user_id) = direct_targets.first().cloned() {
        let partner_display_name = hero_candidates
            .iter()
            .find(|(user_id, _)| user_id == &partner_user_id)
            .map(|(_, display_name)| display_name.as_str())
            .unwrap_or_default();

        return MatrixRoomClassification {
            is_direct: true,
            is_bot_room: is_likely_bot_user(&partner_user_id, partner_display_name),
            direct_chat_other_user_id: partner_user_id,
        };
    }

    match room.active_members_count() {
        2 => {
            if let Some((partner_user_id, display_name)) = hero_candidates.first() {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: is_likely_bot_user(partner_user_id, display_name),
                    direct_chat_other_user_id: partner_user_id.clone(),
                }
            } else {
                MatrixRoomClassification {
                    is_direct: false,
                    is_bot_room: false,
                    direct_chat_other_user_id: String::new(),
                }
            }
        }
        3 => {
            if hero_candidates.len() < 2 {
                return MatrixRoomClassification {
                    is_direct: false,
                    is_bot_room: false,
                    direct_chat_other_user_id: String::new(),
                };
            }

            let (first_user_id, first_display_name) = &hero_candidates[0];
            let (second_user_id, second_display_name) = &hero_candidates[1];
            let first_is_bot = is_likely_bot_user(first_user_id, first_display_name);
            let second_is_bot = is_likely_bot_user(second_user_id, second_display_name);

            if first_is_bot && !second_is_bot {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: false,
                    direct_chat_other_user_id: second_user_id.clone(),
                }
            } else if second_is_bot && !first_is_bot {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: false,
                    direct_chat_other_user_id: first_user_id.clone(),
                }
            } else if first_is_bot && second_is_bot {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: true,
                    direct_chat_other_user_id: first_user_id.clone(),
                }
            } else {
                MatrixRoomClassification {
                    is_direct: false,
                    is_bot_room: false,
                    direct_chat_other_user_id: String::new(),
                }
            }
        }
        _ => MatrixRoomClassification {
            is_direct: false,
            is_bot_room: false,
            direct_chat_other_user_id: String::new(),
        },
    }
}

fn room_list_item_to_summary(room: &RoomListItem) -> MatrixRoomSummary {
    let room_state = room.state();
    let classification = classify_room(room);
    let timestamp = room
        .new_latest_event_timestamp()
        .map(|ts| u64::from(ts.get()))
        .or_else(|| room.recency_stamp().map(u64::from))
        .unwrap_or_default();

    MatrixRoomSummary {
        room_id: room.room_id().to_string(),
        display_name: room
            .cached_display_name()
            .map(|name| name.to_string())
            .or_else(|| room.name())
            .unwrap_or_else(|| room.room_id().to_string()),
        avatar_url: room
            .avatar_url()
            .map(|url| normalize_mxc_uri(url.to_string()))
            .unwrap_or_default(),
        topic: room.topic().unwrap_or_default(),
        direct_chat_other_user_id: classification.direct_chat_other_user_id,
        is_invite: matches!(room_state, RoomState::Invited),
        is_space: room.is_space(),
        is_direct: classification.is_direct,
        is_bot_room: classification.is_bot_room,
        is_encrypted: room.encryption_state().is_encrypted(),
        unread_message_count: room.num_unread_messages(),
        notification_count: room.num_unread_notifications(),
        highlight_count: room.num_unread_mentions(),
        timestamp,
    }
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

pub async fn fetch_own_profile(handle_id: u64) -> Result<MatrixOwnProfile, String> {
    let client = client_for_handle(handle_id)?;
    let user_id = client
        .user_id()
        .map(|user_id| user_id.to_string())
        .unwrap_or_default();

    tracing::debug!(handle_id, user_id, "Fetching own profile via matrix-sdk backend runtime");

    let profile = client
        .account()
        .fetch_user_profile()
        .await
        .map_err(|e| format!("failed to fetch own profile via matrix-sdk: {e}"))?;

    let display_name = profile
        .get_static::<DisplayName>()
        .map_err(|e| format!("failed to parse display name from matrix-sdk profile response: {e}"))?
        .unwrap_or_default();

    let avatar_url = profile
        .get_static::<AvatarUrl>()
        .map_err(|e| format!("failed to parse avatar URL from matrix-sdk profile response: {e}"))?
        .map(|url| normalize_mxc_uri(url.to_string()))
        .unwrap_or_default();

    tracing::debug!(
        handle_id,
        user_id,
        has_display_name = !display_name.is_empty(),
        has_avatar_url = !avatar_url.is_empty(),
        "Fetched own profile via matrix-sdk backend runtime"
    );

    Ok(MatrixOwnProfile {
        display_name,
        avatar_url,
    })
}

pub async fn fetch_room_list(handle_id: u64) -> Result<Vec<MatrixRoomSummary>, String> {
    let snapshot = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| {
            handle
                .room_list_snapshot
                .lock()
                .expect("poisoned matrix room-list snapshot mutex")
                .clone()
        })
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))?;

    tracing::debug!(handle_id, room_count = snapshot.len(), "Fetched matrix room-list snapshot");

    Ok(snapshot)
}

pub async fn fetch_media_content(
    handle_id: u64,
    mxc_uri: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    let client = client_for_handle(handle_id)?;
    let normalized_mxc_uri = normalize_mxc_uri(mxc_uri.trim().to_owned());
    let uri = Box::<MxcUri>::from(normalized_mxc_uri.as_str());
    uri.validate()
        .map_err(|e| format!("invalid mxc uri '{normalized_mxc_uri}': {e}"))?;
    let uri = uri.to_owned();

    let request = if width > 0 && height > 0 {
        let width =
            UInt::try_from(width).map_err(|_| format!("invalid thumbnail width: {width}"))?;
        let height =
            UInt::try_from(height).map_err(|_| format!("invalid thumbnail height: {height}"))?;
        let method = if crop { Method::Crop } else { Method::Scale };

        MediaRequestParameters {
            source: MediaSource::Plain(uri.into()),
            format: MediaFormat::Thumbnail(MediaThumbnailSettings::with_method(
                method, width, height,
            )),
        }
    } else {
        MediaRequestParameters {
            source: MediaSource::Plain(uri.into()),
            format: MediaFormat::File,
        }
    };

    tracing::debug!(
        handle_id,
        mxc_uri = normalized_mxc_uri,
        width,
        height,
        crop,
        "Fetching matrix media content via matrix-sdk backend runtime"
    );

    client
        .media()
        .get_media_content(&request, false)
        .await
        .map_err(|e| format!("failed to fetch matrix media content via matrix-sdk: {e}"))
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
