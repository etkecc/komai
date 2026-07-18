// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// ## Background preloader and timeline cache warming
//
// In matrix-sdk 0.16, building a room timeline from the local SQLite event
// cache is expensive: `room.timeline().await` + `timeline.subscribe().await`
// takes ~2-3 seconds per room, even with no network calls.  The cost comes
// from deserializing cached events and rebuilding the in-memory chunk
// structure inside the SDK.
//
// To avoid this latency on every room switch, we keep Timeline handles alive
// in a shared cache (`preloaded_timelines` on `MatrixBackendHandle`).  This
// preloader eagerly populates that cache in the background after initial sync:
//
// - **Phase 1 ("preload")**: rooms with `timestamp == 0` have no cached
//   events yet.  We build the timeline, paginate backwards to fetch events
//   from the server, and cache the handle.  This also backfills the room-list
//   preview data.
//
// - **Phase 2 ("cache warm")**: rooms with `timestamp > 0` already have
//   events in SQLite from a previous session.  We still need to build a
//   Timeline handle (the expensive part) so it's ready when the user opens
//   the room.  No pagination or server calls are needed — `subscribe()`
//   returns the cached events immediately.
//
// The active timeline loop (`runtime_timeline.rs`) checks this cache before
// calling `room.timeline().await`.  When the user switches away from a room,
// the handle is cached back for potential reuse.

use super::*;
use super::event_summary::summarize_timeline_content;
use super::timeline::build_room_timeline;
use std::time::{Duration as StdDuration, Instant};

/// How many rooms to preload concurrently (Phase 1).
/// Kept low because these rooms need server pagination.
const PRELOAD_CONCURRENCY: usize = 2;

/// How many rooms to warm concurrently (Phase 2).
/// Higher than Phase 1 because no server calls are made — we're only
/// rebuilding timeline state from the local SQLite event cache.
const WARM_CONCURRENCY: usize = 5;

/// Delay between preloading batches to avoid server pressure.
const PRELOAD_BATCH_DELAY: StdDuration = StdDuration::from_millis(300);

/// Delay after initial sync before starting preload.
const PRELOAD_SETTLE_DELAY: StdDuration = StdDuration::from_secs(5);

/// Number of events to paginate backwards for each preloaded room.
const PRELOAD_PAGE_SIZE: u16 = 15;

/// Preview data extracted from the newest event in a preloaded room.
struct RoomPreviewData {
    room_id: String,
    timestamp: u64,
    last_message: String,
    last_message_kind: String,
    last_message_sender_id: String,
    last_message_sender_display_name: String,
    latest_event_id: String,
}

pub fn start_preload(handle_id: u64) -> Result<(), String> {
    let (client, room_list_snapshot, preloaded_timelines, auth_failed) = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get(&handle_id) else {
            return Err(format!(
                "matrix-sdk backend runtime handle {handle_id} is not active"
            ));
        };
        (
            handle.client.clone(),
            Arc::clone(&handle.room_list_snapshot),
            Arc::clone(&handle.preloaded_timelines),
            Arc::clone(&handle.auth_failed),
        )
    };

    std::thread::spawn(move || {
        crate::matrix_backend::ffi::runtime().block_on(run_preload(
            handle_id,
            client,
            room_list_snapshot,
            preloaded_timelines,
            auth_failed,
        ));
    });

    tracing::info!(handle_id, "Started background room timeline preloader");
    Ok(())
}

async fn run_preload(
    handle_id: u64,
    client: Client,
    room_list_snapshot: Arc<Mutex<Vec<MatrixRoomSummary>>>,
    preloaded_timelines: Arc<Mutex<HashMap<String, Timeline>>>,
    auth_failed: Arc<AtomicBool>,
) {
    // Wait for things to settle after initial sync.
    tokio::time::sleep(PRELOAD_SETTLE_DELAY).await;

    if auth_failed.load(Ordering::Relaxed) {
        tracing::info!(
            handle_id,
            "Background preloader: aborting, the session's access token is no longer valid"
        );
        return;
    }

    let (rooms_to_preload, rooms_to_warm) = {
        let snapshot = room_list_snapshot
            .lock()
            .expect("poisoned matrix room-list snapshot mutex");

        let mut preload = Vec::new();
        let mut warm = Vec::new();
        for room in snapshot.iter() {
            if room.is_invite || room.is_space {
                continue;
            }
            if room.timestamp == 0 {
                preload.push(room.room_id.clone());
            } else {
                warm.push(room.room_id.clone());
            }
        }
        (preload, warm)
    };

    if rooms_to_preload.is_empty() && rooms_to_warm.is_empty() {
        tracing::info!(
            handle_id,
            "Background preloader: no rooms to process"
        );
        return;
    }

    tracing::info!(
        handle_id,
        preload_pending = rooms_to_preload.len(),
        warm_pending = rooms_to_warm.len(),
        "Background preloader: starting"
    );

    let started_at = Instant::now();
    let mut preloaded = 0u32;
    let mut skipped = 0u32;
    let mut failed = 0u32;
    let mut previews: Vec<RoomPreviewData> = Vec::new();

    for chunk in rooms_to_preload.chunks(PRELOAD_CONCURRENCY) {
        if auth_failed.load(Ordering::Relaxed) {
            tracing::info!(
                handle_id,
                "Background preloader: aborting, the session's access token is no longer valid"
            );
            return;
        }

        let mut tasks: Vec<(String, _)> = Vec::with_capacity(chunk.len());

        for room_id_str in chunk {
            let client = client.clone();
            let room_id = room_id_str.clone();
            tasks.push((
                room_id_str.clone(),
                tokio::spawn(async move { preload_single_room(&client, &room_id).await }),
            ));
        }

        let mut batch_did_work = false;
        for (room_id, task) in tasks {
            match task.await {
                Ok(PreloadResult::Loaded(preview, timeline)) => {
                    preloaded += 1;
                    batch_did_work = true;
                    preloaded_timelines
                        .lock()
                        .expect("poisoned preloaded timelines mutex")
                        .insert(room_id, timeline);
                    if let Some(p) = preview {
                        previews.push(p);
                    }
                }
                Ok(PreloadResult::AlreadyCached(preview, timeline)) => {
                    skipped += 1;
                    preloaded_timelines
                        .lock()
                        .expect("poisoned preloaded timelines mutex")
                        .insert(room_id, timeline);
                    if let Some(p) = preview {
                        previews.push(p);
                    }
                }
                Ok(PreloadResult::Failed(error)) => {
                    failed += 1;
                    batch_did_work = true;
                    tracing::debug!(
                        handle_id,
                        error,
                        "Background preloader: room failed"
                    );
                }
                Err(join_error) => {
                    failed += 1;
                    batch_did_work = true;
                    tracing::debug!(
                        handle_id,
                        %join_error,
                        "Background preloader: task panicked"
                    );
                }
            }
        }

        if batch_did_work {
            tokio::time::sleep(PRELOAD_BATCH_DELAY).await;
        }
    }

    // Backfill the room list snapshot with preview data.
    if !previews.is_empty() {
        let backfilled = backfill_room_list_snapshot(&room_list_snapshot, &previews);
        if backfilled > 0 {
            tracing::info!(
                handle_id,
                backfilled,
                "Background preloader: backfilled room-list previews"
            );
            // Send targeted updates — does NOT reset the model or scroll position.
            let ffi_updates: Vec<_> = previews
                .iter()
                .filter(|p| p.timestamp > 0)
                .map(|p| crate::ffi::MatrixRoomPreviewUpdate {
                    room_id: p.room_id.clone(),
                    latest_event_id: p.latest_event_id.clone(),
                    last_message: p.last_message.clone(),
                    last_message_kind: p.last_message_kind.clone(),
                    last_message_sender_id: p.last_message_sender_id.clone(),
                    last_message_sender_display_name: p.last_message_sender_display_name.clone(),
                    timestamp: p.timestamp,
                })
                .collect();
            crate::ffi::matrix_notify_room_previews_backfilled(handle_id, ffi_updates);
        }
    }

    // Phase 2: warm the timeline cache for rooms that already have events
    // in SQLite.  No pagination or server calls — just build the Timeline
    // handle so it's ready when the user opens the room.
    let mut warmed = 0u32;
    let mut warm_failed = 0u32;
    let mut warm_skipped = 0u32;
    if !rooms_to_warm.is_empty() {
        tracing::info!(
            handle_id,
            room_count = rooms_to_warm.len(),
            "Background preloader: warming timeline cache"
        );
        let warm_started_at = Instant::now();

        for chunk in rooms_to_warm.chunks(WARM_CONCURRENCY) {
            // Warming itself is local, but empty rooms fall back to server
            // pagination in `warm_single_room` — stop on a dead session too.
            if auth_failed.load(Ordering::Relaxed) {
                tracing::info!(
                    handle_id,
                    "Background preloader: aborting cache warm, the session's access token is \
                     no longer valid"
                );
                return;
            }

            let mut tasks: Vec<(String, _)> = Vec::with_capacity(chunk.len());

            for room_id_str in chunk {
                // Skip rooms that are already cached (e.g., the user already
                // opened this room, or it was cached by Phase 1).
                let already_cached = preloaded_timelines
                    .lock()
                    .expect("poisoned preloaded timelines mutex")
                    .contains_key(room_id_str);
                if already_cached {
                    warm_skipped += 1;
                    continue;
                }

                let client = client.clone();
                let room_id = room_id_str.clone();
                tasks.push((
                    room_id_str.clone(),
                    tokio::spawn(
                        async move { warm_single_room(&client, &room_id).await },
                    ),
                ));
            }

            for (room_id, task) in tasks {
                match task.await {
                    Ok(Ok(timeline)) => {
                        warmed += 1;
                        preloaded_timelines
                            .lock()
                            .expect("poisoned preloaded timelines mutex")
                            .insert(room_id, timeline);
                    }
                    Ok(Err(error)) => {
                        warm_failed += 1;
                        tracing::debug!(
                            handle_id,
                            error,
                            "Background preloader: cache warm failed"
                        );
                    }
                    Err(join_error) => {
                        warm_failed += 1;
                        tracing::debug!(
                            handle_id,
                            %join_error,
                            "Background preloader: cache warm task panicked"
                        );
                    }
                }
            }
        }

        tracing::info!(
            handle_id,
            warmed,
            warm_failed,
            warm_skipped,
            elapsed_ms = warm_started_at.elapsed().as_millis() as u64,
            "Background preloader: cache warming complete"
        );
    }

    let cached_count = preloaded_timelines
        .lock()
        .expect("poisoned preloaded timelines mutex")
        .len();
    let elapsed = started_at.elapsed();
    tracing::info!(
        handle_id,
        preloaded,
        skipped,
        failed,
        warmed,
        warm_failed,
        cached_count,
        elapsed_ms = elapsed.as_millis() as u64,
        "Background preloader: finished"
    );
}

fn backfill_room_list_snapshot(
    room_list_snapshot: &Arc<Mutex<Vec<MatrixRoomSummary>>>,
    previews: &[RoomPreviewData],
) -> u32 {
    let mut snapshot = room_list_snapshot
        .lock()
        .expect("poisoned mutex");
    let mut count = 0u32;
    for preview in previews {
        if let Some(entry) = snapshot.iter_mut().find(|r| r.room_id == preview.room_id) {
            if entry.timestamp == 0 && preview.timestamp > 0 {
                entry.timestamp = preview.timestamp;
                entry.last_message = preview.last_message.clone();
                entry.last_message_kind = preview.last_message_kind.clone();
                entry.last_message_sender_id = preview.last_message_sender_id.clone();
                entry.last_message_sender_display_name =
                    preview.last_message_sender_display_name.clone();
                entry.latest_event_id = preview.latest_event_id.clone();
                count += 1;
            }
        }
    }
    count
}

enum PreloadResult {
    Loaded(Option<RoomPreviewData>, Timeline),
    AlreadyCached(Option<RoomPreviewData>, Timeline),
    Failed(String),
}

async fn preload_single_room(client: &Client, room_id: &str) -> PreloadResult {
    let parsed_room_id = match RoomId::parse(room_id) {
        Ok(id) => id,
        Err(e) => return PreloadResult::Failed(format!("invalid room id: {e}")),
    };

    let room = match client.get_room(&parsed_room_id) {
        Some(room) => room,
        None => return PreloadResult::Failed("room not known to client".to_owned()),
    };

    let timeline = match build_room_timeline(&room).await {
        Ok(t) => t,
        Err(e) => return PreloadResult::Failed(format!("failed to build timeline: {e}")),
    };

    let (items, _stream) = timeline.subscribe().await;

    // If subscribe already returned items, the event cache has data
    // for this room — no need to paginate again.  Still extract
    // preview data so the room list can be backfilled.
    if !items.is_empty() {
        let preview = extract_newest_preview(room_id, &items);
        return PreloadResult::AlreadyCached(preview, timeline);
    }

    if let Err(e) = timeline.paginate_backwards(PRELOAD_PAGE_SIZE).await {
        return PreloadResult::Failed(format!("pagination failed: {e}"));
    }

    // Re-subscribe to get the items populated by pagination.
    let (items, _stream) = timeline.subscribe().await;
    let preview = extract_newest_preview(room_id, &items);

    tracing::debug!(
        room_id,
        item_count = items.len(),
        has_preview = preview.is_some(),
        "Background preloader: preloaded room"
    );
    PreloadResult::Loaded(preview, timeline)
    // The timeline handle is returned to the caller and cached for
    // reuse by the active timeline loop, avoiding the expensive
    // room.timeline() + subscribe() rebuild on room switch.
}

/// Build and return a Timeline handle for a room that already has events
/// in the SQLite event cache.  Calls `room.timeline().await` +
/// `subscribe().await` to rebuild the in-memory state, then returns the
/// handle for caching.  If `subscribe()` yields 0 items (some rooms do
/// this despite having cached events), paginates backwards to populate
/// the handle so the active timeline loop doesn't have to do it live.
async fn warm_single_room(client: &Client, room_id: &str) -> Result<Timeline, String> {
    let parsed_room_id = RoomId::parse(room_id).map_err(|e| format!("invalid room id: {e}"))?;

    let room = client
        .get_room(&parsed_room_id)
        .ok_or_else(|| "room not known to client".to_owned())?;

    let timeline = build_room_timeline(&room)
        .await
        .map_err(|e| format!("failed to build timeline: {e}"))?;

    // Subscribe to populate the in-memory state from the event cache.
    let (items, _stream) = timeline.subscribe().await;

    // Some rooms yield 0 items from subscribe() even though they have
    // events in the SQLite cache.  Paginate so the cached handle has
    // content ready — otherwise the active timeline loop would paginate
    // live, adding ~3s latency on room switch.
    if items.is_empty() {
        if let Err(e) = timeline.paginate_backwards(PRELOAD_PAGE_SIZE).await {
            tracing::warn!(room_id, "Background warm-path pagination failed: {e}");
        }
    }

    Ok(timeline)
}

fn extract_newest_preview(
    room_id: &str,
    items: &Vector<Arc<TimelineItem>>,
) -> Option<RoomPreviewData> {
    let own_user_id: Option<&matrix_sdk::ruma::UserId> = None;

    // Walk backwards to find the newest message-like event.
    for item in items.iter().rev() {
        let Some(event) = item.as_event() else { continue };

        let ts = event.timestamp();
        let timestamp: u64 = u64::from(ts.get());
        if timestamp == 0 {
            continue;
        }

        let Some(eid) = event.event_id() else { continue };
        let event_id = eid.to_string();
        let sender_id = event.sender().to_string();
        let sender_display_name = match event.sender_profile() {
            TimelineDetails::Ready(profile) => profile
                .display_name
                .as_deref()
                .map(str::trim)
                .filter(|name| !name.is_empty())
                .map(ToOwned::to_owned)
                .unwrap_or_default(),
            _ => String::new(),
        };

        let summary = summarize_timeline_content(event.content(), own_user_id, &sender_display_name);
        // Skip state events.
        if matches!(
            summary.kind.as_str(),
            "other_state" | "failed_to_parse_state" | "membership_change"
        ) {
            continue;
        }

        return Some(RoomPreviewData {
            room_id: room_id.to_owned(),
            timestamp,
            last_message: summary.body,
            last_message_kind: summary.kind,
            last_message_sender_id: sender_id,
            last_message_sender_display_name: sender_display_name,
            latest_event_id: event_id,
        });
    }

    None
}
