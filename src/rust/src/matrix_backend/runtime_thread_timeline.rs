// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Thread timeline view — displays a single thread's messages in the
// timeline area instead of the full room timeline.
//
// # Why /relations and not subscribe_to_thread?
//
// The SDK's `RoomEventCache::subscribe_to_thread()` provides a reactive
// subscription with VectorDiff updates, but the event cache does NOT route
// incoming sync events to per-thread subscriptions.  This means the
// subscription fires during explicit pagination but never receives new
// messages from sync — making it useless for live updates.
//
// Instead, we fetch thread events directly from the server using the
// `/relations` endpoint (`Room::relations()`).  This always returns the
// current state of the thread.  The C++ side triggers a refresh on each
// room timeline sync update, which makes the thread view "live":
//
//   user sends → server confirms → sync delivers → room timeline updates
//   → C++ dispatches refresh_thread_timeline → Rust fetches /relations
//   → snapshot updated → C++ notified → model replaced → QML renders
//
// The cost is one HTTP request per sync update while in thread mode.
// For typical sync intervals (2-5 seconds) and small /relations responses
// (threads rarely exceed 50 events), this is acceptable.

use super::*;
use super::event_summary::summarize_sync_timeline_event;

use matrix_sdk::deserialized_responses::TimelineEvent;
use matrix_sdk::ruma::EventId;

/// Per-handle state for an active thread view.  No background threads —
/// all updates are driven by explicit refresh calls from the C++ side.
pub struct ThreadTimelineState {
    pub room_id: String,
    pub thread_root_id: String,
    pub snapshot: Arc<Mutex<Vec<MatrixTimelineItem>>>,
}

// ---------------------------------------------------------------------------
// Public API called from FFI wrappers
// ---------------------------------------------------------------------------

/// Set up thread timeline state for the given thread.  Does not fetch
/// events — call `refresh_thread_timeline` afterwards (on a worker thread)
/// to populate the snapshot.
pub fn subscribe_to_thread_timeline(
    handle_id: u64,
    room_id: &str,
    thread_root_id: &str,
) -> Result<(), String> {
    let room_id = room_id.trim();
    let thread_root_id = thread_root_id.trim();
    if room_id.is_empty() || thread_root_id.is_empty() {
        return Err("room_id and thread_root_id must not be empty".to_owned());
    }

    // Validate the event ID format early.
    let _ = EventId::parse(thread_root_id)
        .map_err(|e| format!("invalid thread root event id '{thread_root_id}': {e}"))?;

    let mut handles = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex");
    let handle = handles.get_mut(&handle_id).ok_or_else(|| {
        format!("matrix-sdk backend runtime handle {handle_id} is not active")
    })?;

    handle.active_thread_subscription = Some(ThreadTimelineState {
        room_id: room_id.to_owned(),
        thread_root_id: thread_root_id.to_owned(),
        snapshot: Arc::new(Mutex::new(Vec::new())),
    });

    tracing::info!(
        handle_id,
        room_id,
        thread_root_id,
        "Initialized thread timeline state"
    );

    Ok(())
}

/// Fetch the latest thread events from the server via `/relations` and
/// update the snapshot.  Notifies C++ if the snapshot changed.
///
/// Called for both the initial load and subsequent live refreshes.
pub async fn refresh_thread_timeline(handle_id: u64) -> Result<(), String> {
    let (room, thread_root_id, snapshot_arc) = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let handle = handles.get(&handle_id).ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} is not active")
        })?;
        let sub = handle.active_thread_subscription.as_ref().ok_or_else(|| {
            format!("matrix-sdk backend runtime handle {handle_id} has no active thread timeline")
        })?;
        let room = handle
            .client
            .get_room(
                &RoomId::parse(&sub.room_id).map_err(|e| format!("invalid room id: {e}"))?,
            )
            .ok_or_else(|| format!("room '{}' not known to client", sub.room_id))?;
        let root_id = EventId::parse(&sub.thread_root_id)
            .map_err(|e| format!("invalid thread root id: {e}"))?;
        (room, root_id, Arc::clone(&sub.snapshot))
    };

    let room_id_str = room.room_id().to_string();
    let thread_root_id_str = thread_root_id.to_string();

    // Fetch the latest thread events from the server.
    let opts = matrix_sdk::room::RelationsOptions {
        from: None,
        dir: matrix_sdk::ruma::api::Direction::Backward,
        limit: Some(UInt::from(50u32)),
        include_relations: matrix_sdk::room::IncludeRelations::AllRelations,
        recurse: true,
    };

    let result = room
        .relations(thread_root_id.clone(), opts)
        .await
        .map_err(|e| format!("failed to fetch thread relations: {e}"))?;

    let own_user_id = room.own_user_id().to_owned();

    let mut events = result.chunk;

    // If we got all events (no more pages), also include the thread root
    // event itself — it is not included in /relations results.
    if result.prev_batch_token.is_none() {
        if let Ok(root_event) = room.load_or_fetch_event(&thread_root_id, None).await {
            events.push(root_event);
        }
    }

    // Events arrive newest-first from backward pagination.  Reverse to
    // chronological order for convert_events_to_items (which reverses
    // again internally for the BottomToTop ListView model).
    events.reverse();

    let items = convert_events_to_items(&events, &room, &own_user_id).await;

    {
        let mut guard = snapshot_arc
            .lock()
            .expect("poisoned thread timeline snapshot mutex");
        if guard.len() == items.len() {
            // Same item count — likely no new events.  Skip the
            // notification to avoid unnecessary model replacements.
            return Ok(());
        }
        *guard = items;
    }

    crate::ffi::matrix_notify_thread_timeline_snapshot_updated(
        handle_id,
        &room_id_str,
        &thread_root_id_str,
    );

    Ok(())
}

/// Fetch the current in-memory thread timeline snapshot.
pub async fn fetch_thread_timeline_snapshot(
    handle_id: u64,
) -> Result<Vec<MatrixTimelineItem>, String> {
    let snapshot = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .and_then(|handle| {
            handle.active_thread_subscription.as_ref().map(|sub| {
                sub.snapshot
                    .lock()
                    .expect("poisoned thread timeline snapshot mutex")
                    .clone()
            })
        })
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no active thread timeline"
            )
        })?;

    Ok(snapshot)
}

/// Paginate backwards (load older events) via `/relations` with the
/// server's pagination token.  Currently re-fetches from the start
/// since we fetch the full thread on each refresh.
pub async fn paginate_thread_timeline_backwards(
    handle_id: u64,
    _num_events: u16,
) -> Result<bool, String> {
    // With the current refresh approach, we always fetch the full recent
    // page of thread events.  True backwards pagination (for very long
    // threads) would need to store and use prev_batch_token.  For now,
    // a refresh is equivalent to pagination.
    refresh_thread_timeline(handle_id).await?;
    Ok(true)
}

/// Clear thread timeline state.
pub fn unsubscribe_from_thread_timeline(handle_id: u64) -> Result<(), String> {
    let mut handles = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex");
    if let Some(handle) = handles.get_mut(&handle_id) {
        if let Some(sub) = handle.active_thread_subscription.take() {
            tracing::info!(
                handle_id,
                room_id = %sub.room_id,
                thread_root_id = %sub.thread_root_id,
                "Cleared thread timeline state"
            );
        }
    }
    Ok(())
}

// ---------------------------------------------------------------------------
// Event → MatrixTimelineItem conversion
// ---------------------------------------------------------------------------

/// Convert a list of raw TimelineEvent into MatrixTimelineItem list.
/// Returns items with index 0 = newest, matching the BottomToTop ListView.
async fn convert_events_to_items(
    events: &[TimelineEvent],
    room: &Room,
    own_user_id: &matrix_sdk::ruma::UserId,
) -> Vec<MatrixTimelineItem> {
    let mut items = Vec::with_capacity(events.len());
    for event in events {
        if let Some(item) = thread_event_to_timeline_item(event, room, own_user_id).await {
            items.push(item);
        }
    }
    // Reverse so index 0 = newest, matching BottomToTop ListView.
    items.reverse();
    items
}

/// Convert a raw TimelineEvent into a MatrixTimelineItem.
async fn thread_event_to_timeline_item(
    event: &TimelineEvent,
    room: &Room,
    own_user_id: &matrix_sdk::ruma::UserId,
) -> Option<MatrixTimelineItem> {
    let event_id = event.event_id()?.to_string();
    let timestamp = event.timestamp.map(|ts| u64::from(ts.0)).unwrap_or(0);

    let raw = event.raw();
    let deserialized = raw.deserialize().ok()?;

    let sender_id = deserialized.sender().to_string();

    // Extract body/kind/media using the existing summarizer.
    let summary = summarize_sync_timeline_event(&deserialized)?;

    // Resolve sender display name and avatar from room membership.
    let (sender_display_name, sender_avatar_url) =
        resolve_member_profile(room, deserialized.sender()).await;

    // Extract thread and reply context from the raw event JSON.
    let (thread_root_id, reply_to_event_id) = extract_relations_from_raw(raw.json().get());

    let is_own = deserialized.sender() == own_user_id;

    Some(MatrixTimelineItem {
        item_id: event_id.clone(),
        event_id,
        delivery_state: String::new(),
        thread_id: thread_root_id,
        is_thread_root: summary.is_thread_root,
        sender_id,
        sender_display_name,
        sender_avatar_url,
        body: summary.body,
        formatted_body: summary.formatted_body,
        reply_event_id: reply_to_event_id,
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
        reply_blurhash: String::new(),
        reactions: Vec::new(),
        reactions_summary: String::new(),
        special_effect_names: summary.special_effect_names,
        item_kind: summary.kind,
        membership_change_kind: String::new(),
        matrix_event_type: summary.matrix_event_type,
        is_edited: false,
        media_url: summary
            .media
            .as_ref()
            .map(|m| m.media_url.clone())
            .unwrap_or_default(),
        thumbnail_url: summary
            .media
            .as_ref()
            .map(|m| m.thumbnail_url.clone())
            .unwrap_or_default(),
        file_name: summary
            .media
            .as_ref()
            .map(|m| m.file_name.clone())
            .unwrap_or_default(),
        mime_type: summary
            .media
            .as_ref()
            .map(|m| m.mime_type.clone())
            .unwrap_or_default(),
        media_width: summary.media.as_ref().map(|m| m.media_width).unwrap_or(0),
        media_height: summary
            .media
            .as_ref()
            .map(|m| m.media_height)
            .unwrap_or(0),
        media_duration_ms: summary
            .media
            .as_ref()
            .map(|m| m.media_duration_ms)
            .unwrap_or(0),
        media_size_bytes: summary
            .media
            .as_ref()
            .map(|m| m.media_size_bytes)
            .unwrap_or(0),
        blurhash: summary
            .media
            .as_ref()
            .map(|m| m.blurhash.clone())
            .unwrap_or_default(),
        media_is_encrypted: summary
            .media
            .as_ref()
            .map(|m| m.media_is_encrypted)
            .unwrap_or(false),
        thumbnail_is_encrypted: summary
            .media
            .as_ref()
            .map(|m| m.thumbnail_is_encrypted)
            .unwrap_or(false),
        is_voice_message: summary.is_voice_message,
        waveform: summary.waveform,
        timestamp,
        is_own,
        state_event_target_user: String::new(),
        state_event_target_user_id: String::new(),
        state_event_detail: String::new(),
        state_event_reason: String::new(),
        state_event_has_sender: false,
        power_level_changes: Vec::new(),
        server_acl_changes: None,
    })
}

/// Resolve a user's display name and avatar URL from room membership.
async fn resolve_member_profile(
    room: &Room,
    user_id: &matrix_sdk::ruma::UserId,
) -> (String, String) {
    match room.get_member_no_sync(user_id).await {
        Ok(Some(member)) => (
            member
                .display_name()
                .unwrap_or_default()
                .to_owned(),
            member
                .avatar_url()
                .map(|u| normalize_mxc_uri(u.to_string()))
                .unwrap_or_default(),
        ),
        _ => (user_id.to_string(), String::new()),
    }
}

/// Extract thread root ID and reply-to event ID from raw event JSON.
fn extract_relations_from_raw(json_str: &str) -> (String, String) {
    let parsed: serde_json::Value = match serde_json::from_str(json_str) {
        Ok(v) => v,
        Err(_) => return (String::new(), String::new()),
    };

    let relates_to = match parsed
        .get("content")
        .and_then(|c| c.get("m.relates_to"))
    {
        Some(r) => r,
        None => return (String::new(), String::new()),
    };

    let thread_root_id = if relates_to
        .get("rel_type")
        .and_then(|v| v.as_str())
        == Some("m.thread")
    {
        relates_to
            .get("event_id")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_owned()
    } else {
        String::new()
    };

    let reply_to_event_id = relates_to
        .get("m.in_reply_to")
        .and_then(|r| r.get("event_id"))
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned();

    (thread_root_id, reply_to_event_id)
}
