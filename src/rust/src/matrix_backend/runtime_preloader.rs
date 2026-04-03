// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use super::event_summary::summarize_timeline_content;
use std::time::{Duration as StdDuration, Instant};

/// How many rooms to preload concurrently.
const PRELOAD_CONCURRENCY: usize = 2;

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
    latest_event_id: String,
}

pub fn start_preload(handle_id: u64) -> Result<(), String> {
    let (client, room_list_snapshot) = {
        let handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get(&handle_id) else {
            return Err(format!(
                "matrix-sdk backend runtime handle {handle_id} is not active"
            ));
        };
        (handle.client.clone(), Arc::clone(&handle.room_list_snapshot))
    };

    std::thread::spawn(move || {
        crate::matrix_backend::ffi::runtime().block_on(run_preload(
            handle_id,
            client,
            room_list_snapshot,
        ));
    });

    tracing::info!(handle_id, "Started background room timeline preloader");
    Ok(())
}

async fn run_preload(
    handle_id: u64,
    client: Client,
    room_list_snapshot: Arc<Mutex<Vec<MatrixRoomSummary>>>,
) {
    // Wait for things to settle after initial sync.
    tokio::time::sleep(PRELOAD_SETTLE_DELAY).await;

    let rooms_to_preload = {
        let snapshot = room_list_snapshot
            .lock()
            .expect("poisoned matrix room-list snapshot mutex");

        snapshot
            .iter()
            .filter(|room| room.timestamp == 0 && !room.is_invite && !room.is_space)
            .map(|room| room.room_id.clone())
            .collect::<Vec<_>>()
    };

    if rooms_to_preload.is_empty() {
        tracing::info!(
            handle_id,
            "Background preloader: no rooms need preloading"
        );
        return;
    }

    tracing::info!(
        handle_id,
        room_count = rooms_to_preload.len(),
        "Background preloader: starting"
    );

    let started_at = Instant::now();
    let mut preloaded = 0u32;
    let mut skipped = 0u32;
    let mut failed = 0u32;
    let mut previews: Vec<RoomPreviewData> = Vec::new();

    for chunk in rooms_to_preload.chunks(PRELOAD_CONCURRENCY) {
        let mut tasks = Vec::with_capacity(chunk.len());

        for room_id_str in chunk {
            let client = client.clone();
            let room_id = room_id_str.clone();
            tasks.push(tokio::spawn(async move {
                preload_single_room(&client, &room_id).await
            }));
        }

        let mut batch_did_work = false;
        for task in tasks {
            match task.await {
                Ok(PreloadResult::Loaded(preview)) => {
                    preloaded += 1;
                    batch_did_work = true;
                    if let Some(p) = preview {
                        previews.push(p);
                    }
                }
                Ok(PreloadResult::AlreadyCached(preview)) => {
                    skipped += 1;
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
                    timestamp: p.timestamp,
                })
                .collect();
            crate::ffi::matrix_notify_room_previews_backfilled(handle_id, ffi_updates);
        }
    }

    let elapsed = started_at.elapsed();
    tracing::info!(
        handle_id,
        preloaded,
        skipped,
        failed,
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
                entry.latest_event_id = preview.latest_event_id.clone();
                count += 1;
            }
        }
    }
    count
}

enum PreloadResult {
    Loaded(Option<RoomPreviewData>),
    AlreadyCached(Option<RoomPreviewData>),
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

    let timeline = match room.timeline().await {
        Ok(t) => t,
        Err(e) => return PreloadResult::Failed(format!("failed to build timeline: {e}")),
    };

    let (items, _stream) = timeline.subscribe().await;

    // If subscribe already returned items, the event cache has data
    // for this room — no need to paginate again.  Still extract
    // preview data so the room list can be backfilled.
    if !items.is_empty() {
        let preview = extract_newest_preview(room_id, &items);
        return PreloadResult::AlreadyCached(preview);
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
    PreloadResult::Loaded(preview)

    // The timeline and stream are dropped here, which triggers the
    // auto-shrink mechanism — in-memory chunks are unloaded but the
    // events remain in the SQLite event cache for future use.
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

        let summary = summarize_timeline_content(event.content(), own_user_id);
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
            last_message_kind: summary.matrix_event_type,
            latest_event_id: event_id,
        });
    }

    None
}
