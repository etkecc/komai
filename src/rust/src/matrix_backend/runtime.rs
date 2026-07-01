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
    encryption::{
        verification::{SasVerification, VerificationRequest},
        recovery::IdentityResetHandle,
    },
    event_handler::EventHandlerDropGuard,
    media::{MediaFormat, MediaRequestParameters, MediaThumbnailSettings},
    ruma::{
        MxcUri, OwnedDeviceId, OwnedEventId, OwnedRoomId, OwnedRoomOrAliasId, OwnedServerName, OwnedUserId, RoomId,
        RoomOrAliasId, RoomVersionId, ServerName, UInt, UserId,
        api::client::{
            membership::{invite_user, leave_room},
            room::{Visibility, create_room, upgrade_room},
        },
        api::client::media::get_content_thumbnail::v3::Method,
        api::client::profile::{AvatarUrl, DisplayName},
        api::error::ErrorKind,
        events::{
            AnyMessageLikeEventContent, InitialStateEvent,
            room::{
                ImageInfo, MediaSource, encryption::RoomEncryptionEventContent,
                member::{MembershipState, RoomMemberEventContent},
                message::{ImageMessageEventContent, MessageType, RoomMessageEventContent},
            },
            tag::{TagInfo, TagName, UserTagName},
        },
        room::RoomType,
    },
    stream::StreamExt,
};
use matrix_sdk_ui::{
    eyeball_im::{Vector, VectorDiff},
    room_list_service::{RoomListItem, filters},
    sync_service::{SyncService, State as SyncServiceState},
    timeline::{
        RoomExt, Timeline, TimelineDetails, TimelineEventItemId, TimelineFocus, TimelineItem,
        VirtualTimelineItem,
    },
};
use tokio::sync::mpsc;

use super::bootstrap;

#[path = "runtime_profile_media.rs"]
mod profile_media;
#[path = "runtime_event_detail/mod.rs"]
mod event_detail;
#[path = "runtime_event_summary/mod.rs"]
mod event_summary;
#[path = "runtime_recovery.rs"]
mod recovery;
#[path = "runtime_device_management.rs"]
mod device_management;
#[path = "runtime_registry.rs"]
mod registry;
#[path = "runtime_verification.rs"]
mod verification;
#[path = "runtime_room_actions.rs"]
mod room_actions;
#[path = "runtime_room_list/mod.rs"]
mod room_list;
#[path = "runtime_room_settings.rs"]
mod room_settings;
#[path = "runtime_timeline/mod.rs"]
mod timeline;
#[path = "runtime_timeline_messaging/mod.rs"]
mod timeline_messaging;
#[path = "runtime_timeline_events.rs"]
mod timeline_events;
#[path = "runtime_timeline_snapshot.rs"]
mod timeline_snapshot;
#[path = "runtime_thread_timeline/mod.rs"]
mod thread_timeline;
#[path = "runtime_user_directory.rs"]
mod user_directory;
#[path = "runtime_room_directory.rs"]
mod room_directory;
#[path = "runtime_notifications.rs"]
mod notifications;
#[path = "runtime_image_packs.rs"]
mod image_packs;
#[path = "runtime_media.rs"]
mod runtime_media;
#[path = "runtime_voip.rs"]
mod runtime_voip;
#[path = "runtime_calls.rs"]
mod runtime_calls;
#[path = "runtime_element_call.rs"]
mod element_call;
#[path = "runtime_rtc.rs"]
mod rtc;
#[path = "runtime_media_proxy.rs"]
mod media_proxy;
#[path = "runtime_media_download.rs"]
mod media_download;
#[path = "runtime_preloader.rs"]
mod preloader;
#[path = "runtime_subscriptions.rs"]
mod subscriptions;

pub use profile_media::{
    fetch_media_content, fetch_own_presence, fetch_own_profile, fetch_room_member_profile,
    fetch_user_profile, ignore_user, remove_own_avatar, remove_own_room_avatar, set_own_display_name,
    set_own_presence, unignore_user, upload_own_avatar, upload_own_room_avatar,
};
pub use recovery::{
    cancel_reset_encryption_identity, continue_reset_encryption_identity_after_approval,
    continue_reset_encryption_identity_with_password, fetch_recovery_status,
    recover_encryption_secrets, setup_recovery, start_reset_encryption_identity,
};
pub use device_management::{
    continue_sign_out_device_with_password, rename_device, start_sign_out_device,
};
pub use registry::{logout_backend, start_restored_backend, stop_backend};
pub use verification::{
    advance_verification_session, block_device, cancel_verification_session,
    clear_verification_session, fetch_user_verification_state, fetch_verification_session,
    start_device_verification, start_self_verification, start_user_verification,
    take_pending_verification_flow_ids, unblock_device, unverify_device,
};
pub use room_actions::{
    ban_user, create_room, invite_user, join_room, kick_user, knock_room, leave_room,
    send_typing_notice, set_own_room_display_name, set_room_is_direct, toggle_room_tag,
    unban_user, upgrade_room,
};
pub use preloader::start_preload;
pub use room_list::{fetch_room_list, start_sync};
pub use subscriptions::{subscribe_room, unsubscribe_room};
pub use room_settings::{
    MatrixChildSpaceEntry,
    apply_room_aliases, apply_room_power_levels, enable_room_encryption, fetch_room_aliases,
    fetch_room_child_spaces, fetch_room_members, fetch_room_power_levels, fetch_room_settings,
    fetch_room_versions_capability,
    remove_room_avatar, set_room_access_rules,
    set_room_history_visibility, set_room_name, set_room_notification_mode, set_room_topic,
    set_user_power_level, upload_room_avatar,
};
pub use media_download::{
    active_timeline_media_download_progress,
    fetch_active_room_timeline_media_content_with_progress,
};
pub use timeline::{
    fetch_active_room_timeline, fetch_active_room_timeline_media_content,
    fetch_room_timeline, fetch_room_timeline_snapshot,
    paginate_active_room_timeline_backwards, select_active_room_timeline,
    set_active_room_timeline_initial_page_size, stop_room_timeline,
};
pub use timeline_messaging::{
    cancel_local_echo, mark_room_as_read, mark_room_event_as_read, mark_room_unread,
    redact_room_event, report_room_event, retry_local_echo, send_room_attachment,
    send_room_edit_message, send_room_message, send_room_message_like_event_json,
    send_room_reply_message, toggle_room_reaction,
};
pub use timeline_events::{
    fetch_active_room_event_content_for_forwarding, fetch_active_room_raw_event_dialog_data,
    fetch_room_frequent_reactions, fetch_room_read_receipts,
    fetch_room_redaction_permissions, fetch_room_thread_roots, pin_room_event, unpin_room_event,
};
pub use runtime_media::{send_room_image, upload_media};
pub use thread_timeline::{
    fetch_thread_timeline_snapshot, paginate_thread_timeline_backwards,
    refresh_thread_timeline, subscribe_to_thread_timeline, unsubscribe_from_thread_timeline,
};
pub use user_directory::search_users;
pub use room_directory::fetch_public_room_directory_page;
pub use notifications::{
    fetch_account_notifications_enabled, fetch_notification_items,
    set_account_notifications_enabled,
};
pub use image_packs::{
    fetch_image_packs, remove_image_pack, save_image_pack, set_image_pack_globally_enabled,
};
pub use element_call::{
    send_element_call_message, start_element_call_session, stop_element_call_session,
};
pub use rtc::decline_rtc_notification;
pub use runtime_voip::fetch_turn_server_info;
pub use runtime_calls::{
    serialize_call_invite, serialize_call_candidates, serialize_call_answer,
    serialize_call_hangup, serialize_call_select_answer, serialize_call_reject,
    serialize_call_negotiate,
};
pub use media_proxy::{
    start_media_proxy, stop_media_proxy,
    is_timeline_media_encrypted, register_timeline_media_proxy_url,
};

pub struct MatrixBackendHandleInfo {
    pub handle_id: u64,
    pub has_session: bool,
    pub auth_type: String,
    pub homeserver_url: String,
    pub user_id: String,
    pub device_id: String,
}

pub struct MatrixOwnProfile {
    pub display_name: String,
    pub avatar_url: String,
}

pub struct MatrixOwnPresence {
    pub state: String,
    pub status_message: String,
}

pub struct MatrixTurnServerInfo {
    pub username: String,
    pub password: String,
    pub uris: Vec<String>,
    pub ttl_seconds: u64,
}

pub struct MatrixRecoveryStatus {
    pub state: String,
    pub has_devices_to_verify_against: bool,
    pub own_device_is_verified: bool,
    pub has_unverified_own_devices: bool,
}

pub struct MatrixSetupRecoveryResult {
    pub recovery_key: String,
}

pub struct MatrixResetEncryptionIdentityResult {
    pub completed: bool,
    pub auth_type: String,
    pub approval_url: String,
}

pub struct MatrixDeviceSignOutResult {
    pub completed: bool,
    pub auth_type: String,
    pub approval_url: String,
}

pub struct MatrixVerificationSession {
    pub flow_id: String,
    pub user_id: String,
    pub device_id: String,
    pub state: String,
    pub error: String,
    pub sender: bool,
    pub is_self_verification: bool,
    pub is_multi_device_verification: bool,
    pub sas_numbers: Vec<u16>,
}

pub struct MatrixUserDevice {
    pub device_id: String,
    pub display_name: String,
    pub verification_state: String,
    pub last_seen_ip: String,
    pub last_seen_ts: u64,
}

pub struct MatrixUserVerificationState {
    pub has_master_key: bool,
    pub user_trust: String,
    pub devices: Vec<MatrixUserDevice>,
}

pub struct MatrixUserProfile {
    pub display_name: String,
    pub avatar_url: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixDirectoryUser {
    pub display_name: String,
    pub user_id: String,
    pub avatar_url: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixPublicRoomDirectoryEntry {
    pub room_id: String,
    pub room_server_name: String,
    pub display_name: String,
    pub avatar_url: String,
    pub topic: String,
    pub canonical_alias: String,
    pub member_count: u64,
    pub is_world_readable: bool,
    pub is_space: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixPublicRoomDirectoryPage {
    pub rooms: Vec<MatrixPublicRoomDirectoryEntry>,
    pub next_batch: String,
    pub total_room_count_estimate: i32,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixRoomSummary {
    pub room_id: String,
    pub latest_event_id: String,
    pub display_name: String,
    pub avatar_url: String,
    pub topic: String,
    pub room_alias: String,
    pub last_message: String,
    pub last_message_kind: String,
    pub last_message_sender_id: String,
    pub last_message_sender_display_name: String,
    pub tags: Vec<String>,
    pub parent_space_room_ids: Vec<String>,
    pub direct_chat_other_user_id: String,
    pub is_invite: bool,
    pub inviter_user_id: String,
    pub inviter_display_name: String,
    pub inviter_avatar_url: String,
    pub invite_reason: String,
    pub is_space: bool,
    pub is_direct: bool,
    pub is_bot_room: bool,
    pub is_encrypted: bool,
    pub is_public: bool,
    pub member_count: u64,
    pub unread_message_count: u64,
    pub notification_count: u64,
    pub highlight_count: u64,
    pub is_marked_unread: bool,
    /// Whether the room currently has an active MatrixRTC (Element Call)
    /// session, derived from non-expired `m.call.member` memberships.
    pub has_active_call: bool,
    /// Number of distinct participants currently in that MatrixRTC session.
    pub active_call_participant_count: u64,
    pub timestamp: u64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixNotificationRequest {
    pub room_id: String,
    pub event_id: String,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixNotificationItem {
    pub room_id: String,
    pub event_id: String,
    pub replacement_event_id: String,
    pub room_name: String,
    pub avatar_url: String,
    pub sender_display_name: String,
    /// Machine-readable notification kind key for C++ translation.
    /// Translated to user-visible text in C++ StateEventText::translateNotificationBody().
    /// When adding or changing notification_kind keys, update that function too.
    pub notification_kind: String,
    pub plain_body: String,
    pub formatted_body: String,
    pub media_mxc_url: String,
    pub is_reply: bool,
    pub is_emote: bool,
    pub is_encrypted: bool,
    pub contains_spoiler: bool,
    pub has_inline_image: bool,
    pub play_sound: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixImagePackImage {
    pub shortcode: String,
    pub body: String,
    pub url: String,
    pub is_emote: bool,
    pub is_sticker: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixImagePack {
    pub source_room_id: String,
    pub state_key: String,
    pub display_name: String,
    pub avatar_url: String,
    pub attribution: String,
    pub is_emote_pack: bool,
    pub is_sticker_pack: bool,
    pub from_space: bool,
    pub is_globally_enabled: bool,
    pub images: Vec<MatrixImagePackImage>,
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
    pub can_change_encryption: bool,
    pub can_upgrade_room: bool,
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct MatrixRoomVersionsCapability {
    /// Server's default room version for new rooms (e.g. "11").
    /// Named `default_version` (not `default`) because `default` is a C++
    /// keyword and cxx does not mangle it.
    pub default_version: String,
    /// Stable room versions the server supports, sorted in ascending numeric
    /// order where possible.
    pub stable: Vec<String>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixRoomAliases {
    pub canonical_alias: String,
    pub alt_aliases: Vec<String>,
    pub published_aliases: Vec<String>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixRoomMember {
    pub user_id: String,
    pub display_name: String,
    pub avatar_url: String,
    pub power_level: i64,
    pub is_invited: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixPowerLevelEntry {
    pub key: String,
    pub level: i64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixRoomPowerLevels {
    pub room_version: String,
    pub creators: Vec<String>,
    pub events: Vec<MatrixPowerLevelEntry>,
    pub users: Vec<MatrixPowerLevelEntry>,
    pub ban: i64,
    pub events_default: i64,
    pub invite: i64,
    pub kick: i64,
    pub redact: i64,
    pub state_default: i64,
    pub users_default: i64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixRoomRedactionPermissions {
    pub can_redact_own: bool,
    pub can_redact_other: bool,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixReadReceiptEntry {
    pub user_id: String,
    pub display_name: String,
    pub avatar_url: String,
    pub timestamp: u64,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MatrixReactionSummary {
    pub key: String,
    pub users: String,
    pub user_ids: Vec<String>,
    pub self_reacted_event: String,
    pub count: u64,
}

#[derive(Clone, Debug, PartialEq)]
pub struct MatrixTimelineItem {
    pub item_id: String,
    pub event_id: String,
    pub transaction_id: String,
    pub delivery_state: String,
    /// Human-readable error string from `EventSendState::SendingFailed`.
    /// Populated only for failed local echoes; empty otherwise.
    pub send_error: String,
    /// Whether the SDK considers the send failure recoverable (retry via
    /// `SendHandle::unwedge` may help). Meaningful only while
    /// `delivery_state == "failed"`.
    pub is_recoverable: bool,
    pub thread_id: String,
    pub is_thread_root: bool,
    pub thread_reply_count: u32,
    pub sender_id: String,
    pub sender_display_name: String,
    pub sender_avatar_url: String,
    pub body: String,
    pub formatted_body: String,
    pub reply_event_id: String,
    pub reply_sender_id: String,
    pub reply_sender_display_name: String,
    pub reply_item_kind: String,
    pub reply_matrix_event_type: String,
    pub reply_body: String,
    pub reply_formatted_body: String,
    pub reply_media_url: String,
    pub reply_thumbnail_url: String,
    pub reply_file_name: String,
    pub reply_mime_type: String,
    pub reply_media_width: u64,
    pub reply_media_height: u64,
    pub reply_media_duration_ms: u64,
    pub reply_media_size_bytes: u64,
    pub reply_blurhash: String,
    pub reactions: Vec<MatrixReactionSummary>,
    pub reactions_summary: String,
    pub special_effect_names: Vec<String>,
    pub item_kind: String,
    pub membership_change_kind: String,
    pub matrix_event_type: String,
    pub is_edited: bool,
    pub media_url: String,
    pub thumbnail_url: String,
    pub file_name: String,
    pub mime_type: String,
    pub media_width: u64,
    pub media_height: u64,
    pub media_duration_ms: u64,
    pub media_size_bytes: u64,
    pub blurhash: String,
    pub media_is_encrypted: bool,
    pub thumbnail_is_encrypted: bool,
    pub is_voice_message: bool,
    pub waveform: Vec<f32>,
    pub timestamp: u64,
    pub is_own: bool,
    pub state_event_target_user: String,
    pub state_event_target_user_id: String,
    pub state_event_detail: String,
    pub state_event_reason: String,
    pub state_event_has_sender: bool,
    /// Cause tag for `unable_to_decrypt` items, empty otherwise. Values are
    /// snake_case names from matrix-sdk's `UtdCause`, e.g.
    /// `"sent_before_we_joined"`, `"withheld_by_sender"`. See
    /// `runtime_event_summary::utd_cause_tag`.
    pub utd_cause: String,
    /// True when the on-wire event was encrypted (either successfully
    /// decrypted, or a UTD). False for cleartext events (including state
    /// events, which are always sent in the clear per Matrix spec) and
    /// local echoes.
    pub is_encrypted_event: bool,
    /// Shield severity recommended by matrix-sdk-ui's `get_shield(false)`.
    /// `""` = no shield, `"red"` = red, `"grey"` = grey.
    pub shield_color: String,
    /// Snake_case `ShieldStateCode` when a shield is present, empty
    /// otherwise. E.g. `"sent_in_clear"`, `"unverified_identity"`,
    /// `"unsigned_device"`, `"authenticity_not_guaranteed"`.
    pub shield_code: String,
    pub power_level_changes: Vec<event_detail::PowerLevelChange>,
    pub server_acl_changes: Option<event_detail::ServerAclChange>,
    /// For `m.room.tombstone` events: the room id the tombstone points at.
    /// Empty for every other event type.
    pub tombstone_replacement_room_id: String,
}

static NEXT_BACKEND_HANDLE_ID: AtomicU64 = AtomicU64::new(1);
const ROOM_LIST_PAGE_SIZE: usize = 100_000;
const ROOM_TIMELINE_INITIAL_PAGE_SIZE: u16 = 15;
const ROOM_TIMELINE_PAGE_SIZE: u16 = 50;
const ROOM_TIMELINE_STOP_POLL_INTERVAL_MS: u64 = 50;
/// Max number of thread timeline subscriptions kept warm.  Mirrors the
/// "default 3 recently-closed timelines" behavior on the room side.
pub(super) const THREAD_SUBSCRIPTION_CACHE_CAP: usize = 3;

struct MatrixBackendHandle {
    client: Client,
    sync_task: Option<MatrixBackendSyncTask>,
    media_proxy: Option<media_proxy::MediaProxyInstance>,
    room_list_snapshot: Arc<Mutex<Vec<MatrixRoomSummary>>>,
    /// Per-room timeline tasks.  Each visited room gets its own loop that
    /// processes timeline diffs from the sync stream and updates the room's
    /// snapshot.  Loops stay alive when the user switches away so hidden
    /// rooms continue to receive real-time updates.
    room_timeline_tasks: HashMap<String, MatrixBackendRoomTimelineTask>,
    /// Which room currently has UI focus.  Used by command-routing functions
    /// (pagination, reactions) that always operate on the active room.
    active_room_id: Option<String>,
    preferred_room_timeline_initial_page_size: u16,
    /// Per-room timeline snapshots.  Each room's loop writes to its own
    /// snapshot so rooms don't interfere with each other.
    room_timeline_snapshots: HashMap<String, Arc<Mutex<Vec<MatrixTimelineItem>>>>,
    /// Shared media lookup across all rooms.  Entries are keyed by item ID
    /// (globally unique) and accumulate from all visited rooms.  NOT cleared
    /// on room switch.
    room_timeline_media_lookup: Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
    /// Cached timeline handles from the background preloader or from rooms the
    /// user previously viewed.  Reusing a cached handle avoids the expensive
    /// `room.timeline().await` + `timeline.subscribe().await` rebuild that
    /// otherwise takes ~2-3 seconds per room switch in matrix-sdk 0.16, even
    /// when events are already in the local SQLite event cache.  The rebuild
    /// cost comes from deserializing cached events and rebuilding the in-memory
    /// timeline chunk structure — keeping the handle alive skips all of that.
    ///
    /// Populated by the background preloader (see `runtime_preloader.rs`) and
    /// by the active timeline loop when the user switches away from a room.
    preloaded_timelines: Arc<Mutex<HashMap<String, Timeline>>>,
    /// Cached thread reply counts per room, populated from
    /// `Room::list_threads()` and incrementally updated when new thread
    /// replies arrive via sync.  Keyed by room_id → event_id → reply_count.
    /// See `docs/architecture/thread-reply-counts.md` for design rationale.
    thread_reply_counts: Arc<Mutex<HashMap<String, HashMap<String, u32>>>>,
    /// Cache of thread timeline subscriptions, keyed by `(room_id,
    /// thread_root_id)`.  Bounded LRU (see `THREAD_SUBSCRIPTION_CACHE_CAP`).
    /// Mirrors the room-timeline reuse pattern: visiting a thread, leaving
    /// the room, then returning skips the SDK `TimelineFocus::Thread`
    /// rebuild and the initial `/relations` fetch — both of which are
    /// expensive and previously ran on every re-entry because thread state
    /// was a single `Option` that got dropped each time.
    ///
    /// Entries for a room are evicted when the room timeline is stopped
    /// (`stop_room_timeline`).
    thread_subscriptions: HashMap<(String, String), thread_timeline::ThreadTimelineState>,
    /// LRU order of `thread_subscriptions` keys, oldest first.  Used to pick
    /// an eviction victim when the cache hits its capacity.
    thread_subscription_lru: Vec<(String, String)>,
    /// The currently-active thread, identifying the entry in
    /// `thread_subscriptions` that receives `Refresh`/`PaginateBackwards`
    /// commands and whose snapshot is served by
    /// `fetch_thread_timeline_snapshot`.  `None` means no thread is
    /// currently being viewed; cached entries may still exist for warm
    /// reuse on re-entry.
    active_thread_key: Option<(String, String)>,
    pending_identity_reset: Arc<Mutex<Option<IdentityResetHandle>>>,
    pending_device_sign_out: Arc<Mutex<Option<PendingDeviceSignOut>>>,
    verification_sessions: Arc<Mutex<HashMap<String, MatrixVerificationSessionEntry>>>,
    pending_verification_flow_ids: Arc<Mutex<Vec<String>>>,
    /// Reference-counted set of rooms that have open UI surfaces (main-window
    /// active tab, detached room windows). The sync-loop-owned reconciler
    /// debounces changes and applies them as sliding-sync room subscriptions,
    /// which pulls the extended `required_state` (notably
    /// `m.room.pinned_events`) into the local state store and enables live
    /// state-event updates via `Room::pinned_event_ids_stream`.
    subscribed_rooms: Arc<subscriptions::SubscribedRooms>,
    _verification_event_handlers: Vec<EventHandlerDropGuard>,
    _call_event_handlers: Vec<EventHandlerDropGuard>,
    _rtc_event_handlers: Vec<EventHandlerDropGuard>,
}

#[derive(Clone)]
struct PendingDeviceSignOut {
    device_id: OwnedDeviceId,
    uiaa_info: matrix_sdk::ruma::api::client::uiaa::UiaaInfo,
}

#[derive(Clone)]
struct MatrixVerificationSessionEntry {
    request: VerificationRequest,
    sas: Option<SasVerification>,
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
    ToggleReaction {
        event_id: String,
        reaction_key: String,
        response: tokio::sync::oneshot::Sender<Result<(), String>>,
    },
    CancelLocalEcho {
        transaction_id: String,
        response: tokio::sync::oneshot::Sender<Result<bool, String>>,
    },
    RetryLocalEcho {
        transaction_id: String,
        response: tokio::sync::oneshot::Sender<Result<(), String>>,
    },
}

#[derive(Clone, Debug)]
struct MatrixTimelineMediaRequest {
    source: MediaSource,
    thumbnail_source: Option<MediaSource>,
    /// Size advertised in the event's media `info.size`, or 0 when absent.
    /// Used as the progress-total fallback when the homeserver's download
    /// response carries no Content-Length (e.g. Synapse's chunked 200).
    size_bytes: u64,
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

fn pending_identity_reset_for_handle(
    handle_id: u64,
) -> Result<Arc<Mutex<Option<IdentityResetHandle>>>, String> {
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| Arc::clone(&handle.pending_identity_reset))
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))
}

fn pending_device_sign_out_for_handle(
    handle_id: u64,
) -> Result<Arc<Mutex<Option<PendingDeviceSignOut>>>, String> {
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| Arc::clone(&handle.pending_device_sign_out))
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))
}

fn verification_sessions_for_handle(
    handle_id: u64,
) -> Result<Arc<Mutex<HashMap<String, MatrixVerificationSessionEntry>>>, String> {
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| Arc::clone(&handle.verification_sessions))
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))
}

fn pending_verification_flow_ids_for_handle(
    handle_id: u64,
) -> Result<Arc<Mutex<Vec<String>>>, String> {
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| Arc::clone(&handle.pending_verification_flow_ids))
        .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} is not active"))
}

pub(super) fn subscribed_rooms_for_handle(
    handle_id: u64,
) -> Result<Arc<subscriptions::SubscribedRooms>, String> {
    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .map(|handle| Arc::clone(&handle.subscribed_rooms))
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
