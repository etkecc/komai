// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Event inspection, frequent reactions, pinned events, permissions, and read receipts.

use super::*;
use super::thread_timeline::extract_relations_from_raw;

use matrix_sdk_base::event_cache::store::EventCacheStoreLockState;
use matrix_sdk::room::ListThreadsOptions;
use matrix_sdk::ruma::{
    EventId,
    api::client::{
        room::get_room_event,
        threads::get_threads::v1::IncludeThreads,
    },
    events::{
        AnySyncMessageLikeEvent, AnySyncTimelineEvent,
        reaction::ReactionEventContent,
        receipt::{ReceiptThread, ReceiptType},
        room::pinned_events::RoomPinnedEventsEventContent,
    },
};
use matrix_sdk_base::deserialized_responses::ThreadSummaryStatus;

// ---------------------------------------------------------------------------
// Frequent reactions
// ---------------------------------------------------------------------------

pub async fn fetch_room_frequent_reactions(
    handle_id: u64,
    room_id: &str,
    lookback_days: i32,
    max_results: u32,
    max_scanned_events: u64,
) -> Result<Vec<String>, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let lookback_days = lookback_days.max(0);
    let max_results = usize::try_from(max_results).unwrap_or(usize::MAX);
    let max_scanned_events = usize::try_from(max_scanned_events).unwrap_or(usize::MAX);
    if max_results == 0 || max_scanned_events == 0 {
        return Ok(Vec::new());
    }

    let client = client_for_handle(handle_id)?;
    let store_guard = match client
        .event_cache_store()
        .lock()
        .await
        .map_err(|e| format!("failed to lock matrix-sdk event cache store: {e}"))?
    {
        EventCacheStoreLockState::Clean(guard) | EventCacheStoreLockState::Dirty(guard) => guard,
    };

    let mut reaction_events = store_guard
        .get_room_events(room.room_id(), Some("m.reaction"), None)
        .await
        .map_err(|e| format!("failed to load matrix-sdk room reactions from event cache: {e}"))?;

    reaction_events.sort_by(|lhs, rhs| rhs.timestamp().cmp(&lhs.timestamp()));

    let own_user_id = room.own_user_id().to_owned();
    let now_ms = u64::from(matrix_sdk::ruma::MilliSecondsSinceUnixEpoch::now().0);
    let cutoff_ms =
        now_ms.saturating_sub(u64::try_from(lookback_days).unwrap_or(0).saturating_mul(86_400_000));

    let mut frequency = HashMap::<String, usize>::new();
    let mut scanned = 0usize;

    for event in reaction_events {
        scanned += 1;
        if scanned > max_scanned_events {
            break;
        }

        let Some(timestamp) = event.timestamp() else {
            continue;
        };
        if u64::from(timestamp.0) < cutoff_ms {
            break;
        }

        let Ok(AnySyncTimelineEvent::MessageLike(AnySyncMessageLikeEvent::Reaction(reaction))) =
            event.raw().deserialize()
        else {
            continue;
        };
        let Some(reaction) = reaction.as_original() else {
            continue;
        };
        if reaction.sender != own_user_id {
            continue;
        }

        let normalized = normalize_reaction_for_comparison(&reaction.content);
        if normalized.is_empty() {
            continue;
        }

        *frequency.entry(normalized).or_default() += 1;
    }

    let mut sorted: Vec<_> = frequency.into_iter().collect();
    sorted.sort_by(|lhs, rhs| rhs.1.cmp(&lhs.1).then_with(|| lhs.0.cmp(&rhs.0)));

    Ok(sorted
        .into_iter()
        .take(max_results)
        .map(|(reaction, _count)| reaction)
        .collect())
}

fn normalize_reaction_for_comparison(content: &ReactionEventContent) -> String {
    content
        .relates_to
        .key
        .chars()
        .filter(|ch| *ch != '\u{FE0F}' && *ch != '\u{FE0E}')
        .collect()
}

// ---------------------------------------------------------------------------
// Thread roots
// ---------------------------------------------------------------------------

pub async fn fetch_room_thread_roots(
    handle_id: u64,
    room_id: &str,
    include: &str,
    from: &str,
    limit: u32,
) -> Result<crate::ffi::MatrixThreadRootsResult, String> {
    use super::event_summary::summarize_sync_timeline_event;

    let room = joined_room_for_handle(handle_id, room_id)?;

    let include_threads = match include {
        "participated" => IncludeThreads::Participated,
        _ => IncludeThreads::All,
    };

    let opts = ListThreadsOptions {
        include_threads,
        from: if from.is_empty() { None } else { Some(from.to_owned()) },
        limit: if limit == 0 { None } else { Some(UInt::from(limit)) },
    };

    let result = room
        .list_threads(opts)
        .await
        .map_err(|e| format!("failed to list threads: {e}"))?;

    let mut items = Vec::new();
    for event in result.chunk {
        let Some(event_id) = event.event_id().map(|id| id.to_string()) else {
            continue;
        };
        let timestamp = event.timestamp.map(|ts| ts.0.into()).unwrap_or(0u64);

        let reply_count = match &event.thread_summary {
            ThreadSummaryStatus::Some(summary) => summary.num_replies,
            _ => 0,
        };

        let raw = event.raw();
        let Some(deserialized) = raw.deserialize().ok() else {
            continue;
        };

        let sender_id = deserialized.sender().to_owned();

        let body = summarize_sync_timeline_event(&deserialized)
            .map(|s| s.body)
            .unwrap_or_default();

        // Resolve sender display name and avatar URL from room membership.
        let (sender_display_name, sender_avatar_url) =
            match room.get_member_no_sync(&sender_id).await {
                Ok(Some(member)) => (
                    member.display_name().unwrap_or_default().to_owned(),
                    member
                        .avatar_url()
                        .map(|u| u.to_string())
                        .unwrap_or_default(),
                ),
                _ => (String::new(), String::new()),
            };

        items.push(crate::ffi::MatrixThreadRootItem {
            event_id,
            sender_id: sender_id.to_string(),
            sender_display_name,
            sender_avatar_url,
            body,
            timestamp,
            reply_count,
        });
    }

    Ok(crate::ffi::MatrixThreadRootsResult {
        items,
        next_batch_token: result.prev_batch_token.unwrap_or_default(),
    })
}

// ---------------------------------------------------------------------------
// Pinned events
// ---------------------------------------------------------------------------

pub async fn pin_room_event(handle_id: u64, room_id: &str, event_id: &str) -> Result<(), String> {
    update_room_pinned_event_ids(handle_id, room_id, event_id, true).await
}

pub async fn unpin_room_event(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(), String> {
    update_room_pinned_event_ids(handle_id, room_id, event_id, false).await
}

async fn update_room_pinned_event_ids(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    should_pin: bool,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot update matrix-sdk room pinned events without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;

    // Get the current pinned event list before doing a read-modify-write.
    // Prefer the state store, which is populated for rooms we've opened (via
    // `runtime_subscriptions::subscribe_room`). Fall back to `/state` in the
    // cold-start window between subscribe and the first sync response, and
    // for the rare case where a pin/unpin is triggered for a room Komai has
    // never opened.
    // The fallback path can fail with a serde error when the room's current
    // `m.room.pinned_events` state event has been redacted: redaction strips
    // `content` to `{}`, and matrix-sdk's `load_pinned_events()` deserializes
    // into the strict (non-`PossiblyRedacted`) ruma type which requires
    // `pinned`. We treat that case as "no current pins" so the caller can
    // proceed; a subsequent pin will then write a well-formed state event.
    let mut pinned_event_ids: Vec<OwnedEventId> = match room.pinned_event_ids() {
        Some(ids) => ids,
        None => match room.load_pinned_events().await {
            Ok(maybe) => maybe.unwrap_or_default(),
            Err(matrix_sdk::Error::SerdeJson(e)) => {
                tracing::warn!(
                    handle_id,
                    room_id = room_id.trim(),
                    error = %e,
                    "m.room.pinned_events state event has unparseable content \
                     (likely redacted); treating the current pin list as empty"
                );
                Vec::new()
            }
            Err(e) => {
                return Err(format!("failed to fetch pinned events from server: {e}"));
            }
        },
    };

    let mut changed = false;
    if should_pin {
        if !pinned_event_ids.iter().any(|candidate| candidate == &parsed_event_id) {
            pinned_event_ids.push(parsed_event_id.clone());
            changed = true;
        }
    } else {
        let original_len = pinned_event_ids.len();
        pinned_event_ids.retain(|candidate| candidate != &parsed_event_id);
        changed = pinned_event_ids.len() != original_len;
    }

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        should_pin,
        changed,
        "Updating matrix-sdk room pinned events"
    );

    if !changed {
        return Ok(());
    }

    room.send_state_event(RoomPinnedEventsEventContent::new(pinned_event_ids))
        .await
        .map(|_| ())
        .map_err(|e| format!("failed to update matrix-sdk room pinned events: {e}"))
}

// ---------------------------------------------------------------------------
// Permissions
// ---------------------------------------------------------------------------

pub async fn fetch_room_redaction_permissions(
    handle_id: u64,
    room_id: &str,
) -> Result<MatrixRoomRedactionPermissions, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let own_user_id = room.own_user_id().to_owned();
    let member = room
        .get_member(&own_user_id)
        .await
        .map_err(|e| format!("failed to fetch matrix-sdk room member permissions: {e}"))?
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} cannot resolve own member state in room {}",
                room_id.trim()
            )
        })?;

    Ok(MatrixRoomRedactionPermissions {
        can_redact_own: member.can_redact_own(),
        can_redact_other: member.can_redact_other(),
    })
}

// ---------------------------------------------------------------------------
// Event inspection
// ---------------------------------------------------------------------------

pub struct RawEventDialogData {
    /// Cleartext form (post-decryption for encrypted events, identical to
    /// the wire form for plaintext events). Empty when `cleartext_error`
    /// is populated (UTDs).
    pub cleartext_json: String,
    /// Populated when the cleartext can't be produced — typically a UTD.
    /// When non-empty, the dialog should still let the user inspect the
    /// wire form to see the encrypted ciphertext.
    pub cleartext_error: String,
    /// Wire form: the JSON the homeserver delivered. For decrypted events
    /// this requires a separate `/rooms/{id}/event/{id}` fetch since
    /// matrix-sdk drops the original ciphertext after decryption. For
    /// UTDs and plaintext events it's already in the cached timeline item
    /// and no network call is made.
    pub wire_json: String,
    /// Populated when the wire-form fetch failed (e.g. server error).
    /// `wire_json` is empty in that case.
    pub wire_error: String,
    /// True when the wire form is byte-equivalent to the cleartext (i.e.
    /// the event was sent in the clear). The dialog uses this to annotate
    /// the wire-form segment with a "(same)" hint.
    pub wire_matches_cleartext: bool,
    pub body: String,
    pub formatted_body: String,
}

/// Extracts the `type` and `content` JSON from a timeline event for forwarding.
///
/// Returns `(event_type, content_json)` — e.g. `("m.room.message", "{\"body\":...}")`.
/// The content JSON is the raw, unmodified event content from the server, preserving
/// all metadata fields (width, height, duration, thumbnail, blurhash, etc.).
pub async fn fetch_active_room_event_content_for_forwarding(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<(String, String), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot extract event content without an event id".to_owned());
    }

    let timeline = room
        .timeline()
        .await
        .map_err(|e| format!("failed to build matrix-sdk room timeline for forwarding: {e}"))?;
    let items = timeline.items().await;

    for item in items.iter() {
        let Some(event) = item.as_event() else {
            continue;
        };
        let Some(current_event_id) = event.event_id() else {
            continue;
        };
        if current_event_id.as_str() != event_id {
            continue;
        }

        let raw_event = event.latest_json().ok_or_else(|| {
            format!("matrix-sdk room event '{event_id}' has no raw JSON available for forwarding")
        })?;

        let parsed: serde_json::Value =
            serde_json::from_str(raw_event.json().get()).map_err(|e| {
                format!("failed to parse raw JSON for matrix-sdk room event '{event_id}': {e}")
            })?;

        let event_type = parsed
            .get("type")
            .and_then(|v| v.as_str())
            .unwrap_or("m.room.message")
            .to_owned();

        let mut content = parsed
            .get("content")
            .ok_or_else(|| {
                format!("matrix-sdk room event '{event_id}' has no content field")
            })?
            .clone();

        // Strip fields that are context-specific to the original room:
        // - m.relates_to: thread/reply relations cause "Relations must be in the same room"
        // - m.mentions: avoids pinging people mentioned in the original message
        if let Some(obj) = content.as_object_mut() {
            obj.remove("m.relates_to");
            obj.remove("m.mentions");
        }

        let content_json = serde_json::to_string(&content).map_err(|e| {
            format!("failed to serialize content of matrix-sdk room event '{event_id}': {e}")
        })?;

        return Ok((event_type, content_json));
    }

    Err(format!(
        "matrix-sdk room timeline for '{}' does not currently include event '{event_id}'",
        room_id.trim(),
    ))
}

pub async fn fetch_active_room_raw_event_dialog_data(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<RawEventDialogData, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err(
            "cannot inspect a matrix-sdk room event without an event id".to_owned(),
        );
    }

    let timeline = room
        .timeline()
        .await
        .map_err(|e| format!("failed to build matrix-sdk room timeline for raw inspection: {e}"))?;
    let items = timeline.items().await;

    for item in items.iter() {
        let Some(event) = item.as_event() else {
            continue;
        };
        let Some(current_event_id) = event.event_id() else {
            continue;
        };
        if current_event_id.as_str() != event_id {
            continue;
        }

        let raw_event = event.latest_json().ok_or_else(|| {
            format!(
                "matrix-sdk room event '{event_id}' does not currently have raw JSON available"
            )
        })?;
        let cleartext_raw_str = raw_event.json().get();

        // Classify the event so we know which payloads to render in the
        // dialog and whether a server fetch is needed for the wire form.
        // matrix-sdk-ui exposes:
        //   - encryption_info().is_some()        => decrypted Megolm event
        //   - content().is_unable_to_decrypt()   => UTD (encrypted, not decrypted)
        //   - neither                            => sent in the clear
        let is_decrypted = event.encryption_info().is_some();
        let is_utd = event.content().is_unable_to_decrypt();

        let (cleartext_json, cleartext_error, body, formatted_body) = if is_utd {
            // UTDs have no cleartext to show — surface the SDK's `UtdCause`
            // (already plumbed through `summarize_msg_like_kind`) so the
            // dialog can explain why decryption failed.
            (
                String::new(),
                "This event is encrypted but couldn't be decrypted on this device.\n\
                 Switch to the wire form to inspect the ciphertext."
                    .to_owned(),
                String::new(),
                String::new(),
            )
        } else {
            let cleartext_pretty = pretty_print_json(cleartext_raw_str)
                .unwrap_or_else(|_| cleartext_raw_str.to_owned());
            let parsed: serde_json::Value = serde_json::from_str(cleartext_raw_str)
                .map_err(|e| {
                    format!("failed to parse raw JSON for matrix-sdk room event '{event_id}': {e}")
                })?;
            let body = parsed
                .get("content")
                .and_then(|c| c.get("body"))
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_owned();
            let formatted_body = parsed
                .get("content")
                .and_then(|c| c.get("formatted_body"))
                .and_then(|v| v.as_str())
                .unwrap_or("")
                .to_owned();
            (cleartext_pretty, String::new(), body, formatted_body)
        };

        // Wire form. Three cases:
        //   - Plaintext event: same as cleartext (annotate "(same)" client-side).
        //   - UTD: matrix-sdk's cached `latest_json` IS the encrypted ciphertext
        //     (it was never decrypted), no server round-trip needed.
        //   - Decrypted event: matrix-sdk drops the ciphertext after
        //     decryption, so fetch the raw event from the homeserver.
        let (wire_json, wire_error, wire_matches_cleartext) = if !is_decrypted {
            // Plaintext or UTD — `latest_json` already holds the wire form.
            let wire_pretty = pretty_print_json(cleartext_raw_str)
                .unwrap_or_else(|_| cleartext_raw_str.to_owned());
            // For UTDs the cleartext is empty/error, so they're never "(same)";
            // for plaintext they always are.
            let matches = !is_utd;
            (wire_pretty, String::new(), matches)
        } else {
            match fetch_wire_form_from_server(&room, event_id).await {
                Ok(json) => (json, String::new(), false),
                Err(err) => (String::new(), err, false),
            }
        };

        return Ok(RawEventDialogData {
            cleartext_json,
            cleartext_error,
            wire_json,
            wire_error,
            wire_matches_cleartext,
            body,
            formatted_body,
        });
    }

    Err(format!(
        "matrix-sdk room timeline for '{}' does not currently include event '{}'",
        room_id.trim(),
        event_id
    ))
}

fn pretty_print_json(raw: &str) -> Result<String, serde_json::Error> {
    let parsed: serde_json::Value = serde_json::from_str(raw)?;
    let mut buf = Vec::new();
    let formatter = serde_json::ser::PrettyFormatter::with_indent(b"    ");
    let mut serializer = serde_json::Serializer::with_formatter(&mut buf, formatter);
    serde::Serialize::serialize(&parsed, &mut serializer)?;
    Ok(String::from_utf8(buf).unwrap_or_else(|_| raw.to_owned()))
}

/// Fetch the on-the-wire JSON for an event via `/rooms/{id}/event/{id}`.
/// Used only for events matrix-sdk has already decrypted — the SDK
/// doesn't keep the original ciphertext, so we re-ask the homeserver.
async fn fetch_wire_form_from_server(
    room: &matrix_sdk::Room,
    event_id: &str,
) -> Result<String, String> {
    let parsed_event_id = EventId::parse(event_id)
        .map_err(|e| format!("invalid event id '{event_id}': {e}"))?;
    let request = get_room_event::v3::Request::new(
        room.room_id().to_owned(),
        parsed_event_id,
    );
    let response = room
        .client()
        .send(request)
        .await
        .map_err(|e| format!("failed to fetch wire form from server: {e}"))?;

    let raw_str = response.event.json().get();
    Ok(pretty_print_json(raw_str).unwrap_or_else(|_| raw_str.to_owned()))
}

// ---------------------------------------------------------------------------
// Read receipts
// ---------------------------------------------------------------------------

pub async fn fetch_room_read_receipts(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
) -> Result<Vec<MatrixReadReceiptEntry>, String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err(
            "cannot inspect matrix-sdk room read receipts without an event id".to_owned(),
        );
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;

    // Get the target event's timestamp (so we can include members who've read
    // *at least* this far — receipts are cumulative) and, if it's a threaded
    // message, its thread root.  A threaded message's read receipts live in
    // the `ReceiptThread::Thread(root)` namespace; some clients (notably
    // bots) only ever send threaded receipts, so checking just Main /
    // Unthreaded would miss them and the dialog would come up empty even
    // though the timeline's delivery indicator (which reads the SDK's
    // thread-scoped receipt tracking) shows "Read".
    let (target_ts, thread_root) = match room.load_or_fetch_event(&parsed_event_id, None).await {
        Ok(event) => {
            let ts = event.timestamp().map(|ts| u64::from(ts.0)).unwrap_or(0);
            let (thread_root_id, _) = extract_relations_from_raw(event.raw().json().get());
            let root = if thread_root_id.is_empty() {
                None
            } else {
                EventId::parse(&thread_root_id).ok()
            };
            (ts, root)
        }
        Err(_) => (0, None),
    };

    // Receipt threads to consult per member.  Always Main + Unthreaded; plus
    // `Thread(root)` when the target is a threaded reply, or `Thread(target)`
    // when it might itself be a thread root (so threaded receipts pointing at
    // its replies still count it as read).
    let mut receipt_threads = vec![ReceiptThread::Main, ReceiptThread::Unthreaded];
    match thread_root {
        Some(root) => receipt_threads.push(ReceiptThread::Thread(root)),
        None => receipt_threads.push(ReceiptThread::Thread(parsed_event_id.clone())),
    }

    let own_user_id = room.own_user_id().to_owned();
    let members = room
        .members(matrix_sdk::RoomMemberships::ACTIVE)
        .await
        .map_err(|e| format!("failed to load room members for read receipts: {e}"))?;

    let mut entries = Vec::new();
    for member in members.iter() {
        if member.user_id() == own_user_id {
            continue;
        }
        for thread in &receipt_threads {
            if let Ok(Some((receipt_event_id, receipt))) =
                room.load_user_receipt(ReceiptType::Read, thread.clone(), member.user_id()).await
            {
                let receipt_ts = receipt.ts.map(|ts| u64::from(ts.0)).unwrap_or(0);
                // Include this member if their receipt targets this exact event,
                // or if the receipt timestamp is >= the event's origin_server_ts
                // (meaning they've read at least this far).
                if receipt_event_id == parsed_event_id || (target_ts > 0 && receipt_ts >= target_ts)
                {
                    entries.push(MatrixReadReceiptEntry {
                        user_id: member.user_id().to_string(),
                        display_name: member
                            .display_name()
                            .map(ToOwned::to_owned)
                            .unwrap_or_else(|| member.user_id().to_string()),
                        avatar_url: member
                            .avatar_url()
                            .map(ToString::to_string)
                            .map(normalize_mxc_uri)
                            .unwrap_or_default(),
                        timestamp: receipt_ts,
                    });
                    break;
                }
            }
        }
    }

    entries.sort_by(|a, b| b.timestamp.cmp(&a.timestamp).then_with(|| a.user_id.cmp(&b.user_id)));

    Ok(entries)
}
