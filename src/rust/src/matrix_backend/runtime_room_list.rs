// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::{HashMap, HashSet};

use super::*;
use super::event_summary::summarize_sync_timeline_event;
use matrix_sdk::{
    deserialized_responses::SyncOrStrippedState,
    ruma::{
        events::{
            AnySyncTimelineEvent, SyncStateEvent, ignored_user_list::IgnoredUserListEventContent,
            room::join_rules::JoinRule,
            space::child::SpaceChildEventContent,
        },
        serde::Raw,
    },
};
use matrix_sdk_base::latest_event::LatestEventValue as BaseLatestEventValue;

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
        crate::matrix_backend::ffi::runtime().block_on(run_sync_loop(
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

async fn run_sync_loop(
    handle_id: u64,
    client: Client,
    room_list_snapshot: Arc<Mutex<Vec<MatrixRoomSummary>>>,
    stop_requested: Arc<AtomicBool>,
) {
    tracing::info!(handle_id, "Running matrix-sdk room-list sync loop");

    let sync_service = match SyncService::builder(client.clone())
        .with_offline_mode()
        .build()
        .await
    {
        Ok(service) => service,
        Err(error) => {
            tracing::warn!(handle_id, %error, "Failed to create matrix-sdk-ui SyncService");
            return;
        }
    };

    let room_list_service = sync_service.room_list_service();

    let room_list = match room_list_service.all_rooms().await {
        Ok(room_list) => room_list,
        Err(error) => {
            tracing::warn!(handle_id, %error, "Failed to acquire matrix-sdk-ui room list");
            return;
        }
    };

    let (entries_stream, entries_controller) =
        room_list.entries_with_dynamic_adapters(ROOM_LIST_PAGE_SIZE);
    if !entries_controller.set_filter(Box::new(filters::new_filter_non_left())) {
        tracing::warn!(handle_id, "Failed to install matrix-sdk-ui room-list filter");
    }

    sync_service.start().await;

    // Spawn the room-subscription reconciler. It applies FFI-driven
    // subscribe/unsubscribe changes to sliding-sync room subscriptions,
    // and forwards per-room `pinned_event_ids_stream` updates to the UI.
    // See `runtime_subscriptions.rs` for the full design.
    let reconciler_stop = Arc::clone(&stop_requested);
    let reconciler_handle = match super::subscribed_rooms_for_handle(handle_id) {
        Ok(subscribed_rooms) => {
            let reconciler_client = client.clone();
            let reconciler_room_list_service = Arc::clone(&room_list_service);
            Some(tokio::spawn(super::subscriptions::run_reconciler(
                handle_id,
                reconciler_client,
                reconciler_room_list_service,
                subscribed_rooms,
                reconciler_stop,
            )))
        }
        Err(error) => {
            tracing::warn!(
                handle_id,
                %error,
                "Failed to acquire subscribed-rooms state; live pinned-events updates will not work"
            );
            None
        }
    };

    let mut state_subscriber = sync_service.state();
    let mut entries_stream = Box::pin(entries_stream);
    let mut ignored_user_list_changes = Some(Box::pin(client.subscribe_to_ignore_user_list_changes()));

    let mut current_values = Vector::<RoomListItem>::new();
    let mut initial_sync_ready_notified = false;
    let mut sync_connected = true;
    // Rooms we have already registered with the SDK's `LatestEvents`
    // subsystem. See `listen_to_latest_events_for_new_rooms` below for
    // why we need to do this ourselves.
    let mut rooms_listening_latest_events: HashSet<OwnedRoomId> = HashSet::new();
    crate::ffi::matrix_notify_ignored_user_list_updated(
        handle_id,
        load_ignored_user_ids(&client).await,
    );

    while !stop_requested.load(Ordering::Relaxed) {
        tokio::select! {
            maybe_diffs = entries_stream.next() => {
                match maybe_diffs {
                    Some(diffs) => {
                        let diffs: Vec<VectorDiff<RoomListItem>> = diffs;
                        for diff in diffs.iter().cloned() {
                            diff.apply(&mut current_values);
                        }

                        listen_to_latest_events_for_new_rooms(
                            &client,
                            &current_values,
                            &mut rooms_listening_latest_events,
                        )
                        .await;

                        let snapshot = build_room_list_snapshot(&current_values).await;
                        let room_count = snapshot.len();
                        let ffi_snapshot = snapshot
                            .iter()
                            .cloned()
                            .map(crate::matrix_backend::ffi::into_ffi_matrix_room_summary)
                            .collect();
                        *room_list_snapshot
                            .lock()
                            .expect("poisoned matrix room-list snapshot mutex") = snapshot;
                        crate::ffi::matrix_notify_room_list_snapshot_updated(
                            handle_id,
                            ffi_snapshot,
                        );

                        if !initial_sync_ready_notified {
                            crate::ffi::matrix_notify_initial_sync_ready(handle_id);
                            initial_sync_ready_notified = true;

                            if let Err(error) = super::preloader::start_preload(handle_id) {
                                tracing::warn!(handle_id, %error, "Failed to start background preloader");
                            }

                            tracing::info!(
                                handle_id,
                                room_count,
                                "Completed initial matrix-sdk-ui room-list sync iteration"
                            );
                        } else {
                            tracing::debug!(
                                handle_id,
                                room_count,
                                "Updated matrix-sdk room-list snapshot"
                            );
                        }
                    }
                    None => {
                        tracing::info!(handle_id, "Matrix-sdk-ui room-list entries stream ended");
                        break;
                    }
                }
            }

            maybe_state = state_subscriber.next() => {
                match maybe_state {
                    Some(SyncServiceState::Running) => {
                        if !sync_connected {
                            sync_connected = true;
                            crate::ffi::matrix_notify_sync_connection_state_changed(handle_id, true);
                            tracing::info!(handle_id, "Matrix-sdk-ui sync service recovered");
                        }
                    }
                    Some(SyncServiceState::Error(error)) => {
                        let error_str = format!("{error}");
                        let is_auth_error = is_auth_failure(&error_str);
                        tracing::warn!(
                            handle_id,
                            %error,
                            is_auth_error,
                            "Matrix-sdk-ui sync service error"
                        );

                        if is_auth_error {
                            crate::ffi::matrix_notify_sync_stopped(
                                handle_id,
                                &error_str,
                                true,
                            );
                            break;
                        }

                        if sync_connected {
                            sync_connected = false;
                            crate::ffi::matrix_notify_sync_connection_state_changed(
                                handle_id,
                                false,
                            );
                        }

                        // Attempt to restart after non-auth errors.
                        sync_service.start().await;
                    }
                    Some(SyncServiceState::Offline) => {
                        tracing::info!(handle_id, "Matrix-sdk-ui sync service entered offline mode");
                        if sync_connected {
                            sync_connected = false;
                            crate::ffi::matrix_notify_sync_connection_state_changed(
                                handle_id,
                                false,
                            );
                        }
                    }
                    Some(SyncServiceState::Terminated) => {
                        tracing::info!(handle_id, "Matrix-sdk-ui sync service terminated");
                        break;
                    }
                    Some(SyncServiceState::Idle) => {
                        tracing::debug!(handle_id, "Matrix-sdk-ui sync service idle");
                    }
                    None => {
                        tracing::info!(handle_id, "Matrix-sdk-ui sync service state stream ended");
                        break;
                    }
                }
            }

            maybe_ignored = async {
                match ignored_user_list_changes.as_mut() {
                    Some(changes) => changes.next().await,
                    None => std::future::pending().await,
                }
            } => {
                match maybe_ignored {
                    Some(user_ids) => {
                        let mut user_ids: Vec<String> = user_ids;
                        user_ids.sort();
                        user_ids.dedup();
                        crate::ffi::matrix_notify_ignored_user_list_updated(handle_id, user_ids.clone());
                        tracing::debug!(
                            handle_id,
                            ignored_user_count = user_ids.len(),
                            "Updated matrix-sdk ignored-user list snapshot"
                        );
                    }
                    None => {
                        ignored_user_list_changes = None;
                        tracing::info!(handle_id, "Matrix-sdk ignored-user list stream ended");
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

    sync_service.stop().await;

    if let Some(handle) = reconciler_handle {
        // The reconciler observes `stop_requested` within ~250ms and exits
        // cleanly. Await it so any in-flight `subscribe_to_rooms` call
        // finishes before we return.
        let _ = handle.await;
    }

    tracing::info!(handle_id, "Matrix-sdk room-list sync loop stopped");
}

/// Register every non-invite, non-space room we know about with the SDK's
/// `LatestEvents` subsystem so it computes and persists `RoomInfo::latest_event`
/// for each.
///
/// The SDK only auto-registers rooms that enter a sliding-sync response window;
/// rooms living outside the window never get a latest event computed, so
/// `matrix_sdk_base::Room::latest_event()` keeps returning
/// `LatestEventValue::None`, which in turn blanks the room-list preview on
/// every snapshot rebuild. Registering every room we've ever seen — even ones
/// not currently visible — keeps the preview stable.
///
/// We do this unconditionally (rather than conditionally on a user setting),
/// so that toggling a preview visibility setting at runtime does not require
/// re-populating caches.
async fn listen_to_latest_events_for_new_rooms(
    client: &Client,
    rooms: &Vector<RoomListItem>,
    already_listening: &mut HashSet<OwnedRoomId>,
) {
    let mut to_register: Vec<OwnedRoomId> = Vec::new();
    for room in rooms.iter() {
        if room.is_space() {
            continue;
        }
        if matches!(room.state(), RoomState::Invited) {
            continue;
        }
        let room_id = room.room_id().to_owned();
        if already_listening.contains(&room_id) {
            continue;
        }
        to_register.push(room_id);
    }

    if to_register.is_empty() {
        return;
    }

    let latest_events = client.latest_events().await;
    for room_id in to_register {
        match latest_events.listen_to_room(&room_id).await {
            Ok(_) => {}
            Err(error) => {
                tracing::debug!(
                    %room_id,
                    %error,
                    "Failed to register room with matrix-sdk latest-events subsystem"
                );
            }
        }
        already_listening.insert(room_id);
    }
}

fn is_auth_failure(error_message: &str) -> bool {
    let lower = error_message.to_lowercase();
    lower.contains("invalid_grant")
        || lower.contains("invalid grant")
        || lower.contains("refresh_token")
        || lower.contains("m_unknown_token")
        || lower.contains("unknown token")
}

async fn build_room_list_snapshot(values: &Vector<RoomListItem>) -> Vec<MatrixRoomSummary> {
    let mut snapshot = Vec::with_capacity(values.len());
    for room in values.iter() {
        snapshot.push(room_list_item_to_summary(room).await);
    }

    // Enrich parent_space_room_ids by reading m.space.child state events from
    // each space. The per-room parent_spaces() call only looks at m.space.parent
    // events set on the child room, which are optional and often absent.
    // The authoritative source is m.space.child events on the space itself.
    let mut child_to_parents: HashMap<String, Vec<String>> = HashMap::new();
    for room in values.iter() {
        if !room.is_space() {
            continue;
        }
        let space_id = room.room_id().to_string();
        if let Ok(child_events) = room.get_state_events_static::<SpaceChildEventContent>().await {
            for raw_event in child_events {
                let child_room_id: Option<String> = match raw_event.deserialize() {
                    Ok(SyncOrStrippedState::Sync(SyncStateEvent::Original(e))) => {
                        Some(e.state_key.to_string())
                    }
                    Ok(SyncOrStrippedState::Stripped(e)) => Some(e.state_key.to_string()),
                    _ => None,
                };
                if let Some(child_room_id) = child_room_id {
                    child_to_parents
                        .entry(child_room_id)
                        .or_default()
                        .push(space_id.clone());
                }
            }
        }
    }

    for summary in &mut snapshot {
        if let Some(extra_parents) = child_to_parents.remove(&summary.room_id) {
            for parent_id in extra_parents {
                if !summary.parent_space_room_ids.contains(&parent_id) {
                    summary.parent_space_room_ids.push(parent_id);
                }
            }
            summary.parent_space_room_ids.sort();
            summary.parent_space_room_ids.dedup();
        }
    }

    snapshot
}

async fn load_ignored_user_ids(client: &Client) -> Vec<String> {
    match client
        .account()
        .account_data::<IgnoredUserListEventContent>()
        .await
    {
        Ok(Some(raw_content)) => match raw_content.deserialize() {
            Ok(content) => {
                let mut user_ids = content
                    .ignored_users
                    .into_keys()
                    .map(|user_id| user_id.to_string())
                    .collect::<Vec<_>>();
                user_ids.sort();
                user_ids.dedup();
                user_ids
            }
            Err(error) => {
                tracing::warn!(%error, "Failed to deserialize matrix-sdk ignored-user list");
                Vec::new()
            }
        },
        Ok(None) => Vec::new(),
        Err(error) => {
            tracing::warn!(%error, "Failed to load matrix-sdk ignored-user list");
            Vec::new()
        }
    }
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

#[derive(Clone, Debug)]
struct RoomHeroCandidate {
    user_id: String,
    display_name: String,
    avatar_url: String,
}

fn room_hero_candidates(room: &RoomListItem) -> Vec<RoomHeroCandidate> {
    let own_user_id = room.own_user_id();
    let mut candidates: Vec<RoomHeroCandidate> = room
        .heroes()
        .into_iter()
        .filter(|hero| hero.user_id != own_user_id)
        .map(|hero| RoomHeroCandidate {
            user_id: hero.user_id.to_string(),
            display_name: hero.display_name.unwrap_or_default(),
            avatar_url: hero
                .avatar_url
                .map(|url| normalize_mxc_uri(url.to_string()))
                .unwrap_or_default(),
        })
        .collect();

    candidates.sort_by(|left, right| left.user_id.cmp(&right.user_id));
    candidates.dedup_by(|left, right| left.user_id == right.user_id);
    candidates
}

fn classify_room(
    room: &RoomListItem,
    hero_candidates: &[RoomHeroCandidate],
) -> MatrixRoomClassification {
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
            .find(|candidate| candidate.user_id == partner_user_id)
            .map(|candidate| candidate.display_name.as_str())
            .unwrap_or_default();

        return MatrixRoomClassification {
            is_direct: true,
            is_bot_room: is_likely_bot_user(&partner_user_id, partner_display_name),
            direct_chat_other_user_id: partner_user_id,
        };
    }

    match room.active_members_count() {
        2 => {
            if let Some(partner) = hero_candidates.first() {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: is_likely_bot_user(&partner.user_id, &partner.display_name),
                    direct_chat_other_user_id: partner.user_id.clone(),
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

            let first = &hero_candidates[0];
            let second = &hero_candidates[1];
            let first_is_bot = is_likely_bot_user(&first.user_id, &first.display_name);
            let second_is_bot = is_likely_bot_user(&second.user_id, &second.display_name);

            if first_is_bot && !second_is_bot {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: false,
                    direct_chat_other_user_id: second.user_id.clone(),
                }
            } else if second_is_bot && !first_is_bot {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: false,
                    direct_chat_other_user_id: first.user_id.clone(),
                }
            } else if first_is_bot && second_is_bot {
                MatrixRoomClassification {
                    is_direct: true,
                    is_bot_room: true,
                    direct_chat_other_user_id: first.user_id.clone(),
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

fn direct_chat_avatar_url(
    hero_candidates: &[RoomHeroCandidate],
    partner_user_id: &str,
) -> Option<String> {
    hero_candidates
        .iter()
        .find(|candidate| candidate.user_id == partner_user_id)
        .and_then(|candidate| (!candidate.avatar_url.is_empty()).then(|| candidate.avatar_url.clone()))
}

// The member-lookup branch exists because the homeserver omits the heroes
// summary for rooms that carry an explicit name, leaving DM rooms without a
// room avatar with no server-provided way to find the partner's avatar.
async fn resolve_room_avatar_url(
    room: &RoomListItem,
    classification: &MatrixRoomClassification,
    hero_candidates: &[RoomHeroCandidate],
) -> String {
    if let Some(url) = room.avatar_url() {
        return normalize_mxc_uri(url.to_string());
    }

    if !classification.is_direct || classification.direct_chat_other_user_id.is_empty() {
        return String::new();
    }

    if let Some(url) =
        direct_chat_avatar_url(hero_candidates, &classification.direct_chat_other_user_id)
    {
        return url;
    }

    let Ok(partner_user_id) =
        matrix_sdk::ruma::UserId::parse(&classification.direct_chat_other_user_id)
    else {
        return String::new();
    };

    match room.get_member_no_sync(&partner_user_id).await {
        Ok(Some(member)) => member
            .avatar_url()
            .map(|uri| normalize_mxc_uri(uri.to_string()))
            .unwrap_or_default(),
        Ok(None) | Err(_) => String::new(),
    }
}

async fn fetch_parent_space_room_ids(room: &RoomListItem) -> Vec<String> {
    let Ok(parent_spaces) = room.parent_spaces().await else {
        tracing::debug!(
            room_id = %room.room_id(),
            "Failed to fetch matrix room parent spaces for room-list summary"
        );
        return Vec::new();
    };

    let mut parent_space_room_ids = parent_spaces
        .filter_map(|result| async move {
            match result {
                Ok(ParentSpace::Reciprocal(parent))
                | Ok(ParentSpace::WithPowerlevel(parent))
                | Ok(ParentSpace::Illegitimate(parent)) => Some(parent.room_id().to_string()),
                Ok(ParentSpace::Unverifiable(parent_room_id)) => Some(parent_room_id.to_string()),
                Err(error) => {
                    tracing::debug!(
                        room_id = %room.room_id(),
                        %error,
                        "Failed to inspect matrix room parent-space relationship"
                    );
                    None
                }
            }
        })
        .collect::<Vec<_>>()
        .await;

    parent_space_room_ids.sort();
    parent_space_room_ids.dedup();
    parent_space_room_ids
}

async fn fetch_room_tags(room: &RoomListItem) -> Vec<String> {
    let Ok(tags) = room.tags().await else {
        tracing::debug!(
            room_id = %room.room_id(),
            "Failed to fetch matrix room tags for room-list summary"
        );
        return Vec::new();
    };

    let Some(tags) = tags else {
        return Vec::new();
    };

    let mut tag_ids = tags.keys().map(ToString::to_string).collect::<Vec<_>>();
    tag_ids.sort();
    tag_ids.dedup();
    tag_ids
}

async fn room_list_item_to_summary(room: &RoomListItem) -> MatrixRoomSummary {
    let room_state = room.state();
    let hero_candidates = room_hero_candidates(room);
    let classification = classify_room(room, &hero_candidates);
    let avatar_url =
        resolve_room_avatar_url(room, &classification, &hero_candidates).await;
    // `matrix_sdk_base::Room::latest_event` is a synchronous inherent method
    // that returns a `BaseLatestEventValue` populated directly from the room's
    // cached `RoomInfo`. We call it via UFCS to avoid dispatching to
    // `matrix_sdk_ui::timeline::RoomExt::latest_event`, which is async and
    // recomputes the value from the event cache.
    let latest_event: BaseLatestEventValue = matrix_sdk_base::Room::latest_event(room);
    let remote_latest_event = match &latest_event {
        BaseLatestEventValue::Remote(event) => Some(event),
        _ => None,
    };
    let latest_preview = remote_latest_event.and_then(|event| {
        let raw_event: Raw<AnySyncTimelineEvent> = event.raw().clone();
        let event = raw_event.deserialize().ok()?;
        summarize_sync_timeline_event(&event)
    });
    // `latest_event_timestamp()` unifies what used to be two separate sources
    // (sliding sync's latest event vs. the event cache's latest timestamp).
    let timestamp = room
        .latest_event_timestamp()
        .map(|ts| u64::from(ts.get()))
        .unwrap_or_default();
    let latest_event_id = latest_event
        .event_id()
        .map(|id| id.to_string())
        .unwrap_or_default();
    let (last_message_sender_id, last_message_sender_display_name) = match remote_latest_event {
        Some(event) => {
            let sender_id = event
                .raw()
                .get_field::<OwnedUserId>("sender")
                .ok()
                .flatten();
            let mut sender_display_name = String::new();

            if let Some(sender_id) = sender_id.as_ref() {
                match room.get_member_no_sync(sender_id).await {
                    Ok(Some(member)) => {
                        sender_display_name = member
                            .display_name()
                            .map(str::trim)
                            .filter(|name| !name.is_empty())
                            .map(ToOwned::to_owned)
                            .unwrap_or_default();
                    }
                    Ok(None) | Err(_) => {}
                }
            }
            (
                sender_id.map(|user_id| user_id.to_string()).unwrap_or_default(),
                sender_display_name,
            )
        }
        None => (String::new(), String::new()),
    };
    let tags = fetch_room_tags(room).await;
    let parent_space_room_ids = fetch_parent_space_room_ids(room).await;

    let is_invite = matches!(room_state, RoomState::Invited);
    let (inviter_user_id, inviter_display_name, inviter_avatar_url, invite_reason) = if is_invite {
        let invite_details = room.invite_details().await.ok();
        let inviter_user_id = invite_details
            .as_ref()
            .and_then(|details| details.inviter.as_ref())
            .map(|inviter| inviter.user_id().to_string())
            .unwrap_or_default();
        let inviter_display_name = invite_details
            .as_ref()
            .and_then(|details| details.inviter.as_ref())
            .map(|inviter| inviter.name().to_owned())
            .filter(|value| !value.trim().is_empty())
            .unwrap_or_default();
        let inviter_avatar_url = invite_details
            .as_ref()
            .and_then(|details| details.inviter.as_ref())
            .and_then(|inviter| inviter.avatar_url())
            .map(|uri| normalize_mxc_uri(uri.to_string()))
            .unwrap_or_default();
        let invite_reason = invite_details
            .as_ref()
            .and_then(|details| details.invitee.event().reason().map(ToOwned::to_owned))
            .unwrap_or_default();
        (inviter_user_id, inviter_display_name, inviter_avatar_url, invite_reason)
    } else {
        (String::new(), String::new(), String::new(), String::new())
    };

    MatrixRoomSummary {
        room_id: room.room_id().to_string(),
        latest_event_id,
        display_name: room
            .cached_display_name()
            .map(|name| name.to_string())
            .or_else(|| room.name())
            .unwrap_or_else(|| room.room_id().to_string()),
        avatar_url,
        topic: room.topic().unwrap_or_default(),
        room_alias: room
            .canonical_alias()
            .map(|alias| alias.to_string())
            .or_else(|| room.alt_aliases().into_iter().next().map(|alias| alias.to_string()))
            .unwrap_or_default(),
        last_message: latest_preview
            .as_ref()
            .map(|preview| preview.body.clone())
            .unwrap_or_default(),
        last_message_kind: latest_preview
            .map(|preview| preview.kind)
            .unwrap_or_default(),
        last_message_sender_id,
        last_message_sender_display_name,
        tags,
        parent_space_room_ids,
        direct_chat_other_user_id: classification.direct_chat_other_user_id,
        is_invite,
        inviter_user_id,
        inviter_display_name,
        inviter_avatar_url,
        invite_reason,
        is_space: room.is_space(),
        is_direct: classification.is_direct,
        is_bot_room: classification.is_bot_room,
        is_encrypted: room.encryption_state().is_encrypted(),
        is_public: matches!(room.join_rule(), Some(JoinRule::Public)),
        member_count: room.active_members_count(),
        unread_message_count: room.num_unread_messages(),
        notification_count: room.num_unread_notifications(),
        highlight_count: room.num_unread_mentions(),
        timestamp,
    }
}
