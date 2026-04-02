// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use super::event_summary::summarize_sync_timeline_event;
use matrix_sdk::ruma::{
    events::{
        AnySyncTimelineEvent, ignored_user_list::IgnoredUserListEventContent,
        room::join_rules::JoinRule,
    },
    serde::Raw,
};

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
    let mut ignored_user_list_changes = Some(Box::pin(client.subscribe_to_ignore_user_list_changes()));

    let mut current_values = Vector::<RoomListItem>::new();
    let mut initial_sync_ready_notified = false;
    let mut sync_connected = true;
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
                        if !sync_connected {
                            sync_connected = true;
                            crate::ffi::matrix_notify_sync_connection_state_changed(handle_id, true);
                            tracing::info!(
                                handle_id,
                                "Matrix-sdk-ui room-list sync recovered"
                            );
                        }
                        if !initial_sync_ready_notified {
                            crate::ffi::matrix_notify_initial_sync_ready(handle_id);
                            initial_sync_ready_notified = true;
                            tracing::info!(
                                handle_id,
                                "Completed initial matrix-sdk-ui room-list sync iteration"
                            );
                        }
                        tracing::debug!(handle_id, "Completed matrix-sdk-ui room-list sync iteration");
                    }
                    Some(Err(error)) => {
                        let error_str = format!("{error}");
                        let is_auth_error = is_auth_failure(&error_str);
                        let is_unknown_pos_error = is_unknown_sync_position_error(&error_str);
                        tracing::warn!(
                            handle_id,
                            %error,
                            is_auth_error,
                            is_unknown_pos_error,
                            "Matrix-sdk-ui room-list sync failed"
                        );

                        if is_auth_error {
                            crate::ffi::matrix_notify_sync_stopped(
                                handle_id,
                                &error_str,
                                true,
                            );
                            break;
                        }

                        if !is_unknown_pos_error && sync_connected {
                            sync_connected = false;
                            crate::ffi::matrix_notify_sync_connection_state_changed(
                                handle_id,
                                false,
                            );
                        }

                        if is_unknown_pos_error {
                            tracing::info!(
                                handle_id,
                                "Restarting matrix-sdk-ui room-list sync stream after M_UNKNOWN_POS"
                            );
                        } else {
                            tracing::info!(
                                handle_id,
                                "Restarting matrix-sdk-ui room-list sync stream after transient failure"
                            );
                            tokio::time::sleep(Duration::from_secs(1)).await;
                        }

                        sync_stream = Box::pin(room_list_service.sync());
                        continue;
                    }
                    None => {
                        tracing::info!(handle_id, "Matrix-sdk-ui room-list sync stream ended");
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
                    Some(mut user_ids) => {
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

    if let Err(error) = room_list_service.stop_sync() {
        tracing::debug!(handle_id, %error, "Stopping matrix-sdk-ui room-list sync returned an error");
    }

    tracing::info!(handle_id, "Matrix-sdk room-list sync loop stopped");
}

fn is_auth_failure(error_message: &str) -> bool {
    let lower = error_message.to_lowercase();
    lower.contains("invalid_grant")
        || lower.contains("invalid grant")
        || lower.contains("refresh_token")
        || lower.contains("m_unknown_token")
        || lower.contains("unknown token")
}

fn is_unknown_sync_position_error(error_message: &str) -> bool {
    let lower = error_message.to_lowercase();
    lower.contains("m_unknown_pos")
        || lower.contains("unknown_pos")
        || lower.contains("connection data unknown to server")
}

async fn build_room_list_snapshot(values: &Vector<RoomListItem>) -> Vec<MatrixRoomSummary> {
    let mut snapshot = Vec::with_capacity(values.len());
    for room in values.iter() {
        snapshot.push(room_list_item_to_summary(room).await);
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
    let latest_preview = room.latest_event().and_then(|event| {
        let raw_event: Raw<AnySyncTimelineEvent> = event.event().raw().clone();
        let event = raw_event.deserialize().ok()?;
        summarize_sync_timeline_event(&event)
    });
    let timestamp = room
        .new_latest_event_timestamp()
        .map(|ts| u64::from(ts.get()))
        .or_else(|| room.recency_stamp().map(u64::from))
        .unwrap_or_default();
    let tags = fetch_room_tags(room).await;
    let parent_space_room_ids = fetch_parent_space_room_ids(room).await;

    MatrixRoomSummary {
        room_id: room.room_id().to_string(),
        latest_event_id: room
            .latest_event()
            .and_then(|event| event.event_id())
            .map(|event_id| event_id.to_string())
            .unwrap_or_default(),
        display_name: room
            .cached_display_name()
            .map(|name| name.to_string())
            .or_else(|| room.name())
            .unwrap_or_else(|| room.room_id().to_string()),
        avatar_url: room
            .avatar_url()
            .map(|url| normalize_mxc_uri(url.to_string()))
            .or_else(|| {
                classification
                    .is_direct
                    .then(|| {
                        direct_chat_avatar_url(
                            &hero_candidates,
                            &classification.direct_chat_other_user_id,
                        )
                    })
                    .flatten()
            })
            .unwrap_or_default(),
        topic: room.topic().unwrap_or_default(),
        last_message: latest_preview
            .as_ref()
            .map(|preview| preview.body.clone())
            .unwrap_or_default(),
        last_message_kind: latest_preview
            .map(|preview| preview.kind)
            .unwrap_or_default(),
        tags,
        parent_space_room_ids,
        direct_chat_other_user_id: classification.direct_chat_other_user_id,
        is_invite: matches!(room_state, RoomState::Invited),
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
