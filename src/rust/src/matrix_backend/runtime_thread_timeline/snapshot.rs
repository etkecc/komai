// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Build and publish the merged thread snapshot: SDK timeline items
//! decorated with `/relations` annotations and read-state markers.

use super::super::*;
use super::read::ThreadReadState;
use super::relations::{ThreadRelationsData, apply_relations_annotations};
use super::super::timeline_snapshot::build_room_timeline_snapshot;

use std::collections::{HashMap, HashSet};

pub(super) fn publish_merged_snapshot(
    handle_id: u64,
    room_id: &str,
    thread_root_id: &str,
    sdk_values: &Vector<Arc<TimelineItem>>,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
    read_state: &ThreadReadState,
    relations_data: &ThreadRelationsData,
    thread_timeline_snapshot: &Arc<Mutex<Vec<MatrixTimelineItem>>>,
    room_timeline_media_lookup: &Arc<Mutex<HashMap<String, MatrixTimelineMediaRequest>>>,
) {
    let relations_items = &relations_data.items;
    // The SDK Thread timeline's own read-receipt tracking is unreliable
    // for delivery indicators (it loads receipts at build time and ignores
    // later ones), so we don't ask it for a "read" set here — `read_state`
    // is re-fetched per rebuild and applied in a pass below.
    let (mut sdk_items, media_lookup) =
        build_room_timeline_snapshot(sdk_values, own_user_id, &HashSet::new(), None);

    // Fix stale local echoes.  In matrix-sdk 0.16, the Thread-focused
    // timeline never transitions local echoes to remote events (sync
    // events don't flow to the thread event cache).  The SDK does
    // update send_state from NotSentYet → Sent via the send queue, but
    // the item stays as a local echo with a delivery indicator forever.
    //
    // When /relations data is available, replace these stale local echoes
    // with the server-authoritative /relations version (the delivery-state
    // pass below restores a "sent"/"read" indicator on it).
    if !relations_items.is_empty() {
        for sdk_item in &mut sdk_items {
            if !sdk_item.is_own {
                continue;
            }
            let is_local_echo = sdk_item.delivery_state.as_str() == "pending";
            if !is_local_echo {
                continue;
            }

            // Match by event_id when it's known (Sent state). Otherwise
            // prefer `unsigned.transaction_id` — the homeserver round-trips
            // the original client-supplied txn_id on events it returns to
            // the sender, so this is a first-party reconciliation key.
            // Fall back to sender + body for servers that strip or omit
            // `unsigned.transaction_id` on /relations responses.
            let matched = if !sdk_item.event_id.is_empty() {
                relations_items.iter().find(|r| r.event_id == sdk_item.event_id)
            } else if !sdk_item.transaction_id.is_empty() {
                relations_items
                    .iter()
                    .find(|r| !r.transaction_id.is_empty()
                        && r.transaction_id == sdk_item.transaction_id)
                    .or_else(|| {
                        relations_items.iter().find(|r| {
                            r.sender_id == sdk_item.sender_id
                                && r.body == sdk_item.body
                                && !r.body.is_empty()
                        })
                    })
            } else {
                relations_items.iter().find(|r| {
                    r.sender_id == sdk_item.sender_id
                        && r.body == sdk_item.body
                        && !r.body.is_empty()
                })
            };

            if let Some(rel_item) = matched {
                *sdk_item = rel_item.clone();
            }
        }
    }

    // sdk_items is reversed (index 0 = newest).  Collect SDK event IDs
    // before merging so we know which /relations items are missing.
    let sdk_event_ids: HashSet<String> = sdk_items
        .iter()
        .filter(|item| !item.event_id.is_empty())
        .map(|item| item.event_id.clone())
        .collect();

    // Also check SDK item_ids (transaction IDs for local echo).
    let sdk_item_ids: HashSet<String> = sdk_items
        .iter()
        .filter(|item| !item.item_id.is_empty())
        .map(|item| item.item_id.clone())
        .collect();

    // Add /relations items that are missing from the SDK timeline.
    let mut added = 0usize;
    for item in relations_items {
        if item.event_id.is_empty() {
            continue;
        }
        if sdk_event_ids.contains(&item.event_id) {
            continue;
        }
        // Skip items whose event_id matches a local echo's item_id
        // (transaction ID) — the SDK version is richer.
        if sdk_item_ids.contains(&item.event_id) {
            continue;
        }
        sdk_items.push(item.clone());
        added += 1;
    }

    if added > 0 {
        // Re-sort: newest first (highest timestamp = index 0).
        sdk_items.sort_by(|a, b| b.timestamp.cmp(&a.timestamp));
    }

    // Delivery indicators for our own messages.  The SDK Thread timeline
    // doesn't reflect post-build read receipts and the /relations converter
    // leaves `delivery_state` empty, so derive it here from the freshly
    // fetched `read_state` (same data the "Read receipts" dialog uses).  A
    // confirmed remote event must also not look like a stuck local echo, so
    // clear `transaction_id`; leave genuinely in-flight echoes alone.
    for item in &mut sdk_items {
        if !item.is_own
            || item.event_id.is_empty()
            || matches!(item.delivery_state.as_str(), "pending" | "failed")
        {
            continue;
        }
        let read = read_state.receipt_event_ids.contains(&item.event_id)
            || (read_state.max_receipt_ts > 0 && item.timestamp <= read_state.max_receipt_ts);
        item.delivery_state = if read { "read".to_owned() } else { "sent".to_owned() };
        item.transaction_id.clear();
    }

    // `transaction_id` on /relations items is populated from the round-tripped
    // `unsigned.transaction_id` purely as a matching key for the local-echo
    // reconciliation above. The QML treats `transactionId.length > 0` as the
    // sole "is local echo" signal (matrix-sdk-ui clears it on remote echo),
    // so leaving it set on a server-confirmed event makes every reply we sent
    // look like a stuck local echo and hides every action that needs an event
    // id (Edit/Reply/React/Forward/...). Clear it for any item that isn't in
    // a transient send state — matching has already happened by this point.
    for item in &mut sdk_items {
        if item.delivery_state.is_empty() && !item.transaction_id.is_empty() {
            item.transaction_id.clear();
        }
    }

    // Fold reaction annotations from /relations onto matching items. The
    // SDK Thread timeline doesn't see reaction sync events in matrix-sdk
    // 0.16 and the raw /relations path doesn't aggregate, so without this
    // a just-sent reaction never shows up as a chip in thread view.
    //
    // This is a *merge*, not an overwrite: /relations is paginated (limit
    // 50 with recurse), so for very long threads it can omit older
    // reactions the SDK already aggregated. Keeping SDK senders preserves
    // those.
    if !relations_data.annotations.is_empty() {
        // Warn when /relations returned a reaction whose parent event isn't
        // in the merged snapshot — that's a silent-failure mode where the
        // chip would never render despite a successful fetch (e.g. a parent
        // that fell outside the 50-event /relations cap, or any future
        // mismatch in how parent_event_id is keyed vs. how we build items).
        for (parent_id, by_key) in &relations_data.annotations {
            if !sdk_items.iter().any(|i| &i.event_id == parent_id) {
                let keys: Vec<&str> = by_key.keys().map(|s| s.as_str()).collect();
                tracing::warn!(
                    parent_event_id = parent_id.as_str(),
                    keys = ?keys,
                    snapshot_size = sdk_items.len(),
                    "Thread reaction bucket has no matching snapshot item"
                );
            }
        }

        for item in &mut sdk_items {
            if item.event_id.is_empty() {
                continue;
            }
            let Some(parent_annotations) = relations_data.annotations.get(&item.event_id) else {
                continue;
            };
            apply_relations_annotations(item, parent_annotations, own_user_id);
        }
    }

    let snapshot_count = sdk_items.len();
    {
        let mut guard = thread_timeline_snapshot
            .lock()
            .expect("poisoned thread timeline snapshot mutex");
        *guard = sdk_items;
    }
    {
        let mut media_guard = room_timeline_media_lookup
            .lock()
            .expect("poisoned room timeline media lookup mutex");
        media_guard.extend(media_lookup);
    }

    if snapshot_count > 0 {
        crate::ffi::matrix_notify_thread_timeline_snapshot_updated(
            handle_id, room_id, thread_root_id,
        );
    }
}
