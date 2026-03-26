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
    Room,
    RoomState,
    media::{MediaFormat, MediaRequestParameters, MediaThumbnailSettings},
    ruma::{
        MxcUri, OwnedRoomId, OwnedRoomOrAliasId, OwnedServerName, OwnedUserId, RoomId,
        RoomOrAliasId, ServerName, UInt, UserId,
        api::client::{
            error::ErrorKind,
            membership::{ban_user, invite_user, kick_user, leave_room, unban_user},
            room::{Visibility, create_room},
        },
        api::client::media::get_content_thumbnail::v3::Method,
        api::client::profile::{AvatarUrl, DisplayName},
        events::{
            AnyMessageLikeEventContent, InitialStateEvent,
            room::{MediaSource, encryption::RoomEncryptionEventContent, message::RoomMessageEventContent},
        },
        room::RoomType,
        serde::Raw,
    },
    stream::StreamExt,
};
use matrix_sdk_ui::{
    RoomListService,
    eyeball_im::{Vector, VectorDiff},
    room_list_service::{RoomListItem, filters},
    timeline::{
        RoomExt, TimelineDetails, TimelineEventItemId, TimelineItem, VirtualTimelineItem,
    },
};
use tokio::sync::mpsc;

use super::bootstrap;

#[path = "runtime_profile_media.rs"]
mod profile_media;
#[path = "runtime_event_summary.rs"]
mod event_summary;
#[path = "runtime_registry.rs"]
mod registry;
#[path = "runtime_room_actions.rs"]
mod room_actions;
#[path = "runtime_room_list.rs"]
mod room_list;
#[path = "runtime_room_settings.rs"]
mod room_settings;
#[path = "runtime_timeline.rs"]
mod timeline;

pub use profile_media::{
    fetch_media_content, fetch_own_profile, fetch_user_profile, ignore_user, remove_own_avatar,
    set_own_display_name, unignore_user, upload_own_avatar,
};
pub use registry::{start_restored_backend, stop_backend};
pub use room_actions::{
    ban_user, create_room, invite_user, join_room, kick_user, knock_room, leave_room, unban_user,
};
pub use room_list::{fetch_room_list, start_sync};
pub use room_settings::{
    enable_room_encryption, fetch_room_settings, remove_room_avatar, set_room_access_rules,
    set_room_history_visibility, set_room_name, set_room_notification_mode, set_room_topic,
    upload_room_avatar,
};
pub use timeline::{
    fetch_active_room_timeline, fetch_active_room_timeline_media_content,
    fetch_room_redaction_permissions, paginate_active_room_timeline_backwards, redact_room_event,
    select_active_room_timeline, send_room_attachment, send_room_edit_message,
    send_room_message, send_room_reply_message, toggle_room_reaction,
};

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

pub struct MatrixUserProfile {
    pub display_name: String,
    pub avatar_url: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixRoomSummary {
    pub room_id: String,
    pub display_name: String,
    pub avatar_url: String,
    pub topic: String,
    pub last_message: String,
    pub last_message_kind: String,
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
pub struct MatrixRoomSettings {
    pub room_id: String,
    pub room_name: String,
    pub room_topic: String,
    pub room_avatar_url: String,
    pub room_version: String,
    pub member_count: u64,
    pub notifications: i32,
    pub join_rule: String,
    pub history_visibility: String,
    pub allowed_room_ids: Vec<String>,
    pub parent_space_room_ids: Vec<String>,
    pub guest_access: bool,
    pub is_encrypted: bool,
    pub can_change_name: bool,
    pub can_change_topic: bool,
    pub can_change_avatar: bool,
    pub can_change_join_rules: bool,
    pub can_change_history_visibility: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixRoomRedactionPermissions {
    pub can_redact_own: bool,
    pub can_redact_other: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixReactionSummary {
    pub key: String,
    pub users: String,
    pub self_reacted_event: String,
    pub count: u64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixTimelineItem {
    pub item_id: String,
    pub event_id: String,
    pub sender_id: String,
    pub sender_display_name: String,
    pub sender_avatar_url: String,
    pub body: String,
    pub reply_event_id: String,
    pub reply_sender_display_name: String,
    pub reply_body: String,
    pub reactions: Vec<MatrixReactionSummary>,
    pub reactions_summary: String,
    pub item_kind: String,
    pub is_edited: bool,
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
    room_timeline_media_lookup: Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
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

#[derive(Clone, Debug)]
struct MatrixTimelineMediaRequest {
    source: MediaSource,
    thumbnail_source: Option<MediaSource>,
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

fn room_for_handle(handle_id: u64, room_id: &str) -> Result<Room, String> {
    let client = client_for_handle(handle_id)?;
    let parsed_room_id = parse_room_id(room_id)?;
    client
        .get_room(&parsed_room_id)
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} cannot see room {}", room_id.trim()))
}

fn joined_room_for_handle(handle_id: u64, room_id: &str) -> Result<Room, String> {
    let room = room_for_handle(handle_id, room_id)?;

    if room.state() != RoomState::Joined {
        return Err(format!(
            "matrix-sdk backend runtime handle {handle_id} cannot operate on non-joined room {} (state: {:?})",
            room_id.trim(),
            room.state()
        ));
    }

    Ok(room)
}

fn trim_reason(reason: &str) -> Option<String> {
    let reason = reason.trim();
    if reason.is_empty() {
        None
    } else {
        Some(reason.to_owned())
    }
}

fn parse_room_or_alias_id(room_id_or_alias: &str) -> Result<OwnedRoomOrAliasId, String> {
    RoomOrAliasId::parse(room_id_or_alias.trim())
        .map_err(|e| format!("invalid room id or alias '{room_id_or_alias}': {e}"))
}

fn parse_room_id(room_id: &str) -> Result<OwnedRoomId, String> {
    RoomId::parse(room_id.trim()).map_err(|e| format!("invalid room id '{room_id}': {e}"))
}

fn parse_user_id(user_id: &str) -> Result<OwnedUserId, String> {
    UserId::parse(user_id.trim()).map_err(|e| format!("invalid user id '{user_id}': {e}"))
}

fn parse_via_server_names(via: &[String]) -> Result<Vec<OwnedServerName>, String> {
    via.iter()
        .map(|server_name| {
            ServerName::parse(server_name.trim())
                .map_err(|e| format!("invalid via server name '{}': {e}", server_name))
        })
        .collect()
}

fn matrix_errcode(error: &matrix_sdk::Error) -> String {
    match error.client_api_error_kind() {
        Some(ErrorKind::Forbidden { .. }) => "M_FORBIDDEN".to_owned(),
        Some(ErrorKind::NotFound) => "M_NOT_FOUND".to_owned(),
        Some(ErrorKind::UnknownToken { .. }) => "M_UNKNOWN_TOKEN".to_owned(),
        _ => String::new(),
    }
}

fn normalize_mxc_uri(uri: String) -> String {
    if uri.is_empty() || uri.contains("://") {
        return uri;
    }

    format!("mxc://{uri}")
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
