// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Async event loop driving a single thread timeline subscription.

use super::*;
use super::super::*;
use super::items::request_reply_details;
use super::read::fetch_thread_read_state;
use super::relations::{
    ThreadRelationsData, augment_annotations_from_room_cache, fetch_relations_events,
};
use super::snapshot::publish_merged_snapshot;

use std::collections::{HashMap, HashSet};
use std::sync::{Arc, Mutex};
use std::sync::atomic::{AtomicBool, Ordering};
use std::time::Duration;

use matrix_sdk::Client;
use matrix_sdk::ruma::RoomId;
use tokio::sync::mpsc;

pub(super) async fn run_thread_timeline_loop(
    handle_id: u64,
    client: Client,
    room_id: String,
    thread_root_id: String,
    thread_timeline_snapshot: Arc<Mutex<Vec<MatrixTimelineItem>>>,
    room_timeline_media_lookup: Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
    mut commands: mpsc::UnboundedReceiver<ThreadTimelineCommand>,
    stop_requested: Arc<AtomicBool>,
) {
    tracing::info!(handle_id, room_id, thread_root_id, "Running thread timeline loop");

    let parsed_room_id = match RoomId::parse(&room_id) {
        Ok(rid) => rid,
        Err(error) => {
            tracing::warn!(handle_id, room_id, %error, "Invalid room id for thread timeline task");
            return;
        }
    };

    let parsed_thread_root_id = match EventId::parse(&thread_root_id) {
        Ok(eid) => eid,
        Err(error) => {
            tracing::warn!(handle_id, thread_root_id, %error, "Invalid thread root event id");
            return;
        }
    };

    let Some(room) = client.get_room(&parsed_room_id) else {
        tracing::warn!(handle_id, room_id, "Room not known to client for thread timeline");
        return;
    };

    let own_user_id = client.user_id();

    // -----------------------------------------------------------------------
    // Build the SDK TimelineFocus::Thread timeline
    // -----------------------------------------------------------------------
    let focus = TimelineFocus::Thread {
        root_event_id: parsed_thread_root_id.clone(),
    };

    let timeline = match room.timeline_builder().with_focus(focus).build().await {
        Ok(t) => Arc::new(t),
        Err(error) => {
            tracing::warn!(
                handle_id, room_id, thread_root_id, %error,
                "Failed to build thread-focused timeline"
            );
            return;
        }
    };

    // Subscribe to the room event cache so we get pinged for events that
    // arrive in the room but don't surface as room timeline diffs — most
    // importantly remote reactions on thread messages (their parent is
    // in-thread, so the SDK Live timeline emits no diff and the C++-side
    // refresh trigger never fires).  `Timeline::build()` above has already
    // called `client.event_cache().subscribe()`, so the cache is live.
    let (room_event_cache, _event_cache_drop_handles) = match room.event_cache().await {
        Ok(rec) => rec,
        Err(error) => {
            tracing::warn!(
                handle_id, room_id, thread_root_id, %error,
                "Failed to acquire room event cache for thread timeline; \
                 reactions arriving via sync may not surface until another \
                 room event triggers a refresh"
            );
            return;
        }
    };
    let mut room_event_subscriber = match room_event_cache.subscribe().await {
        Ok((_initial_events, sub)) => sub,
        Err(error) => {
            tracing::warn!(
                handle_id, room_id, thread_root_id, %error,
                "Failed to subscribe to room event cache for thread timeline"
            );
            return;
        }
    };

    let (items, stream) = timeline.subscribe().await;
    let mut current_values = items;

    // /relations data for the merge.  Seeded by an initial fetch below so
    // we don't sit on a stale SDK snapshot while waiting for sync to nudge
    // us — `TimelineFocus::Thread` doesn't receive sync events in
    // matrix-sdk 0.16, so without this the view can show only the cached
    // root and miss events posted in another session.
    let mut relations_data = ThreadRelationsData::default();

    // Publish the initial snapshot from the SDK timeline.
    let read_state = fetch_thread_read_state(&room, &parsed_thread_root_id).await;
    publish_merged_snapshot(
        handle_id, &room_id, &thread_root_id,
        &current_values, own_user_id,
        &read_state,
        &relations_data,
        &thread_timeline_snapshot,
        &room_timeline_media_lookup,
    );

    // Initial /relations fetch — bypasses the Refresh debounce so the
    // first paint reflects server state, not just whatever the SDK Thread
    // event cache happened to have.
    match fetch_relations_events(&room, &parsed_thread_root_id).await {
        Ok(mut data) => {
            augment_annotations_from_room_cache(
                &mut data, &room_event_cache, &parsed_thread_root_id,
            ).await;
            tracing::info!(
                handle_id, room_id, thread_root_id,
                relations_count = data.items.len(),
                annotation_parents = data.annotations.len(),
                "Initial thread /relations fetch"
            );
            relations_data = data;
            let read_state = fetch_thread_read_state(&room, &parsed_thread_root_id).await;
            publish_merged_snapshot(
                handle_id, &room_id, &thread_root_id,
                &current_values, own_user_id,
                &read_state,
                &relations_data,
                &thread_timeline_snapshot,
                &room_timeline_media_lookup,
            );
        }
        Err(error) => {
            tracing::warn!(
                handle_id, room_id, thread_root_id, %error,
                "Initial thread /relations fetch failed"
            );
        }
    }

    // Fetch reply details for events with unavailable reply content.
    let mut reply_detail_fetch_requested: HashSet<OwnedEventId> = HashSet::new();
    request_reply_details(&current_values, &timeline, &mut reply_detail_fetch_requested);

    // Kick off initial backwards pagination to load thread history.
    let initial_paginate_fut = timeline.paginate_backwards(50u16);
    tokio::pin!(initial_paginate_fut);
    let mut initial_paginate_pending = true;

    // Debounce state for Refresh commands.  We wait for a quiet period
    // before actually fetching `/relations` to avoid a flood of rebuilds
    // during rapid room timeline updates (pagination, local echo, etc.).
    let mut refresh_deadline: Option<tokio::time::Instant> = None;
    let far_future = tokio::time::Instant::now() + Duration::from_secs(86400);

    let mut stream = Box::pin(stream);
    while !stop_requested.load(Ordering::Relaxed) {
        tokio::select! {
            result = &mut initial_paginate_fut, if initial_paginate_pending => {
                initial_paginate_pending = false;
                if let Err(error) = result {
                    tracing::warn!(
                        handle_id, room_id, thread_root_id, %error,
                        "Initial thread timeline pagination failed"
                    );
                }
            }

            maybe_diffs = stream.next() => {
                match maybe_diffs {
                    Some(diffs) => {
                        let diffs: Vec<VectorDiff<Arc<TimelineItem>>> = diffs;
                        for diff in diffs.iter().cloned() {
                            diff.apply(&mut current_values);
                        }

                        let read_state = fetch_thread_read_state(&room, &parsed_thread_root_id).await;
                        publish_merged_snapshot(
                            handle_id, &room_id, &thread_root_id,
                            &current_values, own_user_id,
                            &read_state,
                            &relations_data,
                            &thread_timeline_snapshot,
                            &room_timeline_media_lookup,
                        );

                        request_reply_details(&current_values, &timeline, &mut reply_detail_fetch_requested);

                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            diff_count = diffs.len(),
                            "Thread timeline SDK diff"
                        );
                    }
                    None => {
                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            "Thread timeline stream ended"
                        );
                        break;
                    }
                }
            }

            // Wake up when ANY event lands in the room event cache from
            // sync.  This is the only signal that catches reactions on
            // thread messages, since their parent is in-thread and the
            // SDK Live timeline emits no diff for them.  We schedule a
            // debounced refresh (the same one the C++ Refresh command
            // uses) — `/relations` is bounded by the thread root, so the
            // extra fetches for unrelated room activity are cheap and
            // coalesce naturally.
            maybe_update = room_event_subscriber.recv() => {
                use matrix_sdk::event_cache::{EventsOrigin, RoomEventCacheUpdate};
                use tokio::sync::broadcast::error::RecvError;
                let should_schedule = match maybe_update {
                    Ok(RoomEventCacheUpdate::UpdateTimelineEvents(diffs)) => {
                        matches!(diffs.origin, EventsOrigin::Sync)
                    }
                    // A read receipt landing in the room can change one of our
                    // own thread messages from "Received" to "Read"; typing
                    // notifications can't, so don't refresh for those.
                    Ok(RoomEventCacheUpdate::AddEphemeralEvents { events }) => {
                        events.iter().any(|e| {
                            e.get_field::<String>("type").ok().flatten().as_deref()
                                == Some("m.receipt")
                        })
                    }
                    Ok(_) => false,
                    Err(RecvError::Lagged(n)) => {
                        tracing::warn!(
                            handle_id, room_id, thread_root_id, lagged = n,
                            "Thread room event cache subscriber lagged; \
                             scheduling refresh to recover"
                        );
                        true
                    }
                    Err(RecvError::Closed) => {
                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            "Thread room event cache subscriber closed"
                        );
                        false
                    }
                };
                if should_schedule && refresh_deadline.is_none() {
                    refresh_deadline = Some(
                        tokio::time::Instant::now() + Duration::from_millis(300)
                    );
                }
            }

            maybe_command = commands.recv() => {
                match maybe_command {
                    Some(ThreadTimelineCommand::PaginateBackwards(page_size)) => {
                        tracing::info!(
                            handle_id, room_id, thread_root_id, page_size,
                            "Paginating thread timeline backwards"
                        );
                        if let Err(error) = timeline.paginate_backwards(page_size).await {
                            tracing::warn!(
                                handle_id, room_id, thread_root_id, page_size, %error,
                                "Failed to paginate thread timeline backwards"
                            );
                            crate::ffi::matrix_notify_thread_timeline_snapshot_updated(
                                handle_id, &room_id, &thread_root_id,
                            );
                        }
                    }
                    Some(ThreadTimelineCommand::Refresh) => {
                        // Debounce: coalesce a burst of Refresh commands
                        // (room-timeline syncs, pagination, …) into a
                        // single /relations fetch. Kept short because
                        // local echoes for thread replies stay stuck
                        // until /relations reconciles them — this is the
                        // upper bound on "React button missing after
                        // sending a thread reply".
                        if refresh_deadline.is_none() {
                            refresh_deadline = Some(
                                tokio::time::Instant::now() + Duration::from_millis(300)
                            );
                        }
                    }
                    None => {
                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            "Thread timeline command channel closed"
                        );
                        break;
                    }
                }
            }

            // Debounced /relations fetch fires when the deadline arrives.
            _ = tokio::time::sleep_until(
                refresh_deadline.unwrap_or(far_future)
            ), if refresh_deadline.is_some() => {
                refresh_deadline = None;

                match fetch_relations_events(
                    &room,
                    &parsed_thread_root_id,
                ).await {
                    Ok(mut data) => {
                        augment_annotations_from_room_cache(
                            &mut data, &room_event_cache, &parsed_thread_root_id,
                        ).await;
                        let prev_count = relations_data.items.len();
                        tracing::info!(
                            handle_id, room_id, thread_root_id,
                            relations_count = data.items.len(),
                            annotation_parents = data.annotations.len(),
                            prev_count,
                            "Refreshed thread /relations"
                        );
                        relations_data = data;
                    }
                    Err(error) => {
                        tracing::warn!(
                            handle_id, room_id, thread_root_id, %error,
                            "Failed to fetch thread /relations"
                        );
                    }
                }

                let read_state = fetch_thread_read_state(&room, &parsed_thread_root_id).await;
                publish_merged_snapshot(
                    handle_id, &room_id, &thread_root_id,
                    &current_values, own_user_id,
                    &read_state,
                    &relations_data,
                    &thread_timeline_snapshot,
                    &room_timeline_media_lookup,
                );
            }
        }
    }

    tracing::info!(handle_id, room_id, thread_root_id, "Thread timeline loop exiting");
}
