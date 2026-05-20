// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! `/relations` server fetch, annotation parsing, and reaction merging.
//! When Synapse drops some reactions from `/relations` we augment them
//! from the local room event cache (additive — deduped on parent/key/sender).

use super::super::*;
use super::items::raw_event_to_timeline_item;

use std::collections::{BTreeMap, HashMap};

use matrix_sdk::Room;

/// `/relations` result split into things that become timeline rows
/// (replies, attachments, …) and reaction aggregations to fold onto
/// existing items. Reaction events themselves are filtered out of the
/// items list so they don't render as stray "Reactions updated" rows.
#[derive(Default)]
pub(super) struct ThreadRelationsData {
    pub(super) items: Vec<MatrixTimelineItem>,
    /// parent_event_id -> key -> sender_id -> reaction_event_id
    pub(super) annotations: HashMap<String, HashMap<String, HashMap<String, String>>>,
}


/// Merge `/relations`-derived annotations into an item's `reactions`
/// array. Existing SDK reactions are preserved (their per-sender
/// information may be richer than what we can derive from raw events),
/// and `/relations` adds any senders the SDK hasn't seen — ensuring that
/// a just-sent reaction surfaces as a chip in thread view.
pub(super) fn apply_relations_annotations(
    item: &mut MatrixTimelineItem,
    annotations: &HashMap<String, HashMap<String, String>>,
    own_user_id: Option<&matrix_sdk::ruma::UserId>,
) {
    // Tooltip text stays compact; the full sender list rides on `user_ids`
    // and is consumed by the reaction-details dialog.
    const MAX_TOOLTIP_USERS: usize = 10;

    // Seed per-key sender maps from the existing SDK summary. The
    // SDK-built `users` field is a newline-joined list (truncated with a
    // "… and N more" sentinel beyond MAX_DISPLAYED_USERS) — we recover
    // what we can; the truncated tail is fine to lose since /relations
    // typically supplies the full sender list anyway.
    let mut by_key: BTreeMap<String, BTreeMap<String, String>> = BTreeMap::new();
    let own_str = own_user_id.map(|u| u.as_str().to_owned());

    for reaction in item.reactions.drain(..) {
        let mut senders = BTreeMap::<String, String>::new();
        for line in reaction.users.split('\n') {
            let trimmed = line.trim();
            if trimmed.is_empty() || trimmed.starts_with('…') {
                continue;
            }
            senders.insert(trimmed.to_owned(), String::new());
        }
        // Restore own user's reaction event id so a future redact can
        // target it. `__local__` means it's still a local echo with no
        // server-confirmed event id; treat it as "we reacted, no event id".
        if let Some(own) = own_str.as_deref() {
            if !reaction.self_reacted_event.is_empty() {
                let event_id = if reaction.self_reacted_event == "__local__" {
                    String::new()
                } else {
                    reaction.self_reacted_event.clone()
                };
                senders.insert(own.to_owned(), event_id);
            }
        }
        by_key.entry(reaction.key).or_default().extend(senders);
    }

    for (key, senders) in annotations {
        let entry = by_key.entry(key.clone()).or_default();
        for (sender, reaction_event_id) in senders {
            // Prefer the /relations-supplied reaction event id over an
            // SDK placeholder (empty string) so the redact path has a
            // real target.
            match entry.get(sender) {
                Some(existing) if !existing.is_empty() => continue,
                _ => {
                    entry.insert(sender.clone(), reaction_event_id.clone());
                }
            }
        }
    }

    item.reactions = by_key
        .into_iter()
        .filter(|(_, senders)| !senders.is_empty())
        .map(|(key, senders)| {
            let total = senders.len();
            let user_ids: Vec<String> = senders.keys().cloned().collect();
            let mut users_list: Vec<String> = user_ids
                .iter()
                .take(MAX_TOOLTIP_USERS)
                .cloned()
                .collect();
            if total > MAX_TOOLTIP_USERS {
                users_list.push(format!("… and {} more", total - MAX_TOOLTIP_USERS));
            }
            let users = users_list.join("\n");
            let self_reacted_event = own_str
                .as_deref()
                .and_then(|own| senders.get(own).cloned())
                .unwrap_or_default();
            MatrixReactionSummary {
                key,
                users,
                user_ids,
                self_reacted_event,
                count: total as u64,
            }
        })
        .collect();

    item.reactions_summary = item
        .reactions
        .iter()
        .map(|r| format!("{} {}", r.key, r.count))
        .collect::<Vec<_>>()
        .join("  ");
}


// ---------------------------------------------------------------------------
// /relations fetch + raw event conversion
// ---------------------------------------------------------------------------

/// Augment `/relations` annotations with reactions found in the persisted
/// room event cache.
///
/// Synapse's `GET /_matrix/client/v1/rooms/{roomId}/relations/{eventId}`
/// with `recurse=true` is observed (2026-05) to silently drop reactions
/// on some thread reply events — same homeserver, no redaction, the
/// reaction event is present in our local room cache (and visible in the
/// main timeline). We work around it by querying the persisted room
/// cache directly for `m.annotation` relations of every item in the
/// thread snapshot, then folding any reactions Synapse omitted into the
/// annotation map.
///
/// The annotation map is keyed by `(parent_event_id, key, sender)`, so
/// reactions present in BOTH `/relations` and the cache deduplicate
/// naturally. We prefer the existing entry (which may carry a richer
/// reaction event id) and only insert when the sender is missing.
pub(super) async fn augment_annotations_from_room_cache(
    data: &mut ThreadRelationsData,
    room_event_cache: &matrix_sdk::event_cache::RoomEventCache,
    thread_root_id: &OwnedEventId,
) {
    use matrix_sdk::ruma::events::relation::RelationType;

    // Collect parent event ids to look up: the thread root + every item
    // we already pulled via /relations. Items missing an event_id (local
    // echoes) can't have reactions yet, so we skip them.
    let mut parents: Vec<OwnedEventId> = Vec::with_capacity(data.items.len() + 1);
    parents.push(thread_root_id.clone());
    for item in &data.items {
        if item.event_id.is_empty() { continue; }
        if let Ok(eid) = OwnedEventId::try_from(item.event_id.clone()) {
            parents.push(eid);
        }
    }

    let mut added = 0usize;
    let mut probed = 0usize;
    for parent in &parents {
        let related = match room_event_cache
            .find_event_relations(parent, Some(vec![RelationType::Annotation]))
            .await
        {
            Ok(events) => events,
            Err(error) => {
                tracing::warn!(
                    parent = %parent, %error,
                    "find_event_relations failed; skipping parent"
                );
                continue;
            }
        };
        probed += related.len();
        for event in related {
            let raw = event.raw().json().get();
            let Some(ann) = parse_annotation_event(raw) else { continue; };
            let by_key = data.annotations.entry(ann.parent_event_id).or_default();
            let by_sender = by_key.entry(ann.key).or_default();
            if !by_sender.contains_key(&ann.sender_id) {
                by_sender.insert(ann.sender_id, ann.reaction_event_id);
                added += 1;
            }
        }
    }

    // Only log when the workaround actually did something — silent in the
    // common case where /relations was complete, loud when it backfilled.
    if added > 0 {
        tracing::info!(
            thread_root = %thread_root_id,
            parents_checked = parents.len(),
            cache_reactions_seen = probed,
            added_to_annotations = added,
            "Augmented thread annotations from room event cache"
        );
    }
}


/// Fetch thread events from the server via `/relations` and split them
/// into timeline rows + reaction aggregations.  Replies/attachments become
/// basic `MatrixTimelineItem` rows (no SDK-processed reply previews; that's
/// added in `publish_merged_snapshot` if the SDK has the same item).
/// Edits and reactions don't get their own rows — edits fold into the
/// original via `unsigned.relations.replace`, reactions get aggregated
/// onto their parent's `reactions` array during merge.
pub(super) async fn fetch_relations_events(
    room: &Room,
    thread_root_id: &OwnedEventId,
) -> Result<ThreadRelationsData, String> {
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
        if let Ok(root_event) = room.load_or_fetch_event(thread_root_id, None).await {
            events.push(root_event);
        }
    }

    // Events arrive newest-first from backward pagination.  Reverse to
    // chronological order.
    events.reverse();

    let mut data = ThreadRelationsData::default();
    for event in &events {
        let raw_json = event.raw().json().get();

        // Edits fold into the original via `unsigned.relations.replace`,
        // so an `m.replace` event must not become its own timeline row —
        // it would render as a stray "* …" with the Matrix edit fallback.
        if event_relation_rel_type(raw_json) == Some("m.replace") {
            continue;
        }

        // Reactions become aggregations on the parent's `reactions` array
        // rather than rows of their own. `recurse: true` brings reactions
        // on thread replies into this fetch — the SDK Thread timeline
        // doesn't see them via sync in matrix-sdk 0.16, so this path is
        // how a just-sent reaction surfaces in thread view.
        if let Some(annotation) = parse_annotation_event(raw_json) {
            data.annotations
                .entry(annotation.parent_event_id)
                .or_default()
                .entry(annotation.key)
                .or_default()
                .insert(annotation.sender_id, annotation.reaction_event_id);
            continue;
        }

        if let Some(item) = raw_event_to_timeline_item(event, room, &own_user_id).await {
            data.items.push(item);
        }
    }

    Ok(data)
}


pub(super) struct ParsedAnnotation {
    parent_event_id: String,
    key: String,
    sender_id: String,
    reaction_event_id: String,
}

/// Parse an `m.reaction` event JSON into its annotation parts. Returns
/// `None` for any event that isn't a usable reaction (wrong type, missing
/// fields, or redacted — redactions strip `content.m.relates_to`).
pub(super) fn parse_annotation_event(json_str: &str) -> Option<ParsedAnnotation> {
    let parsed: serde_json::Value = serde_json::from_str(json_str).ok()?;

    if parsed.get("type").and_then(|v| v.as_str()) != Some("m.reaction") {
        return None;
    }

    let relates_to = parsed.get("content").and_then(|c| c.get("m.relates_to"))?;
    if relates_to.get("rel_type").and_then(|v| v.as_str()) != Some("m.annotation") {
        return None;
    }

    let parent_event_id = relates_to.get("event_id").and_then(|v| v.as_str())?.to_owned();
    let key = relates_to.get("key").and_then(|v| v.as_str())?.to_owned();
    let sender_id = parsed.get("sender").and_then(|v| v.as_str())?.to_owned();
    let reaction_event_id = parsed
        .get("event_id")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned();

    if parent_event_id.is_empty() || key.is_empty() || sender_id.is_empty() {
        return None;
    }

    Some(ParsedAnnotation {
        parent_event_id,
        key,
        sender_id,
        reaction_event_id,
    })
}

/// Read the `content.m.relates_to.rel_type` of a raw event, if any.
pub(super) fn event_relation_rel_type(json_str: &str) -> Option<&'static str> {
    // Cheap path: parse just enough to look up the rel_type. We map known
    // values to `'static` strings so callers don't have to deal with
    // owned-vs-borrowed lifetimes.
    let parsed: serde_json::Value = serde_json::from_str(json_str).ok()?;
    let rel_type = parsed
        .get("content")?
        .get("m.relates_to")?
        .get("rel_type")?
        .as_str()?;
    match rel_type {
        "m.replace" => Some("m.replace"),
        "m.annotation" => Some("m.annotation"),
        "m.thread" => Some("m.thread"),
        "m.reference" => Some("m.reference"),
        _ => None,
    }
}
