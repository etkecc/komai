// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;

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
                        crate::ffi::matrix_notify_room_list_snapshot_updated(handle_id);

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

fn build_room_list_snapshot(values: &Vector<RoomListItem>) -> Vec<MatrixRoomSummary> {
    values.iter().map(room_list_item_to_summary).collect()
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
