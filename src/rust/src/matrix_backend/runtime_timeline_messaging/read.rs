// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Read receipts and the room-level marked-unread flag.

use super::*;

pub async fn mark_room_event_as_read(
    handle_id: u64,
    room_id: &str,
    event_id: &str,
    public_receipt: bool,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    let event_id = event_id.trim();
    if event_id.is_empty() {
        return Err("cannot mark a matrix-sdk room event as read without an event id".to_owned());
    }

    let parsed_event_id =
        EventId::parse(event_id).map_err(|e| format!("invalid event id '{event_id}': {e}"))?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        event_id,
        public_receipt,
        "Marking matrix-sdk room event as read"
    );

    // When the user has opted out of advertising read state, we still need the
    // homeserver to consider the room read for *this* user (so unread counts
    // clear, `m.fully_read` advances, `m.marked_unread` is cleared) — we just
    // switch the ephemeral receipt from public to private so it isn't
    // federated outward or surfaced to other users via /sync.
    let mut receipts = Receipts::new().fully_read_marker(Some(parsed_event_id.clone()));
    if public_receipt {
        receipts = receipts.public_read_receipt(Some(parsed_event_id.clone()));
    } else {
        receipts = receipts.private_read_receipt(Some(parsed_event_id.clone()));
    }

    room.send_multiple_receipts(receipts)
        .await
        .map_err(|e| format!("failed to mark matrix-sdk room event as read: {e}"))?;

    // Optimistically anchor `read_receipts.latest_active` to the just-acked
    // event and zero the counts.  matrix-sdk's `Room::send_multiple_receipts`
    // returns once the HTTP request is acknowledged but never touches local
    // state — it relies on sliding sync to echo the receipt back through
    // the receipts extension, at which point `compute_unread_counts`
    // recomputes the active receipt and the unread counts.
    //
    // In the wild we've seen rooms get stuck: HTTP returns 200, so the
    // server has the receipt, but the local `latest_active` never moves
    // past some old event and the badge keeps reappearing every time
    // `compute_unread_counts` runs.  We don't fully understand the
    // trigger — receipts work fine for most rooms, so this isn't a
    // blanket "Synapse never echoes own receipts" problem.  It might be
    // a one-shot dropped echo, or specific to rooms where the latest
    // events are state events (e.g. `m.room.server_acl` churn from a
    // moderation bot), or something else entirely.  Posting a fresh
    // message clears it via the implicit-receipt path in matrix-sdk's
    // `select_best_receipt` (events we sent count as receipts on
    // themselves), but explicit `m.read` does not have that fallback.
    //
    // Whatever the trigger, the HTTP just returned 200 so the server
    // has the receipt; mirror that into RoomInfo (with
    // `RoomInfoNotableUpdateReasons::READ_RECEIPT` so observers
    // update) and persist via `state_store().save_changes` (otherwise
    // the stale state reloads on next startup).  Future syncs still
    // recompute correctly: any new event past `parsed_event_id` is
    // counted from this anchor by `find_and_process_events`.  For rooms
    // that weren't stuck this is a no-op once sync arrives with the
    // real echo.
    {
        use matrix_sdk_base::{
            RoomInfoNotableUpdateReasons, StateChanges, read_receipts::LatestReadReceipt,
        };

        let mut info = room.clone_info();
        let mut receipts = info.read_receipts().clone();
        receipts.latest_active = Some(LatestReadReceipt { event_id: parsed_event_id });
        receipts.num_unread = 0;
        receipts.num_notifications = 0;
        receipts.num_mentions = 0;
        info.set_read_receipts(receipts);

        // matrix-sdk's `send_multiple_receipts` internally calls `set_unread_flag(false)`
        // after posting the receipt, but that — like our explicit `mark_room_unread` —
        // only POSTs the account-data write.  Local `is_marked_unread` would otherwise
        // stay `true` until the sliding-sync echo, leaving a manually-marked-unread
        // room visually stuck after the user opens it.  Mirror the clear locally too.
        if matrix_sdk_base::Room::is_marked_unread(&room)
            && let Err(error) = patch_marked_unread(&mut info, false)
        {
            tracing::warn!(
                room_id = room_id.trim(),
                %error,
                "Failed to apply optimistic marked-unread clear; UI will refresh on the next sync echo"
            );
        }

        let mut state_changes = StateChanges::default();
        state_changes.add_room(info.clone());
        if let Err(error) = room.client().state_store().save_changes(&state_changes).await {
            tracing::warn!(
                room_id = room_id.trim(),
                %error,
                "Failed to persist optimistic read-receipt update; \
                 in-memory state is still applied, but the badge may \
                 reappear after restart"
            );
        }

        room.update_room_info(|_| (info, RoomInfoNotableUpdateReasons::READ_RECEIPT)).await;
    }

    Ok(())
}

pub(super) fn patch_marked_unread(
    info: &mut matrix_sdk_base::RoomInfo,
    unread: bool,
) -> Result<(), String> {
    let mut json = serde_json::to_value(&*info).map_err(|e| {
        format!("failed to serialize RoomInfo for optimistic marked-unread update: {e}")
    })?;
    let base = json
        .get_mut("base_info")
        .and_then(JsonValue::as_object_mut)
        .ok_or_else(|| "RoomInfo serialization missing base_info object".to_owned())?;
    base.insert("is_marked_unread".to_owned(), JsonValue::Bool(unread));
    base.insert(
        "is_marked_unread_source".to_owned(),
        JsonValue::String("Stable".to_owned()),
    );
    *info = serde_json::from_value(json).map_err(|e| {
        format!("failed to deserialize RoomInfo after optimistic marked-unread update: {e}")
    })?;
    Ok(())
}

pub(super) async fn optimistically_flip_marked_unread(
    room: &matrix_sdk::Room,
    room_id: &str,
    unread: bool,
) {
    use matrix_sdk_base::{RoomInfoNotableUpdateReasons, StateChanges};

    let mut info = room.clone_info();
    if let Err(error) = patch_marked_unread(&mut info, unread) {
        tracing::warn!(
            room_id = room_id.trim(),
            %error,
            "Failed to apply optimistic marked-unread update; UI will refresh on the next sync echo"
        );
        return;
    }

    let mut state_changes = StateChanges::default();
    state_changes.add_room(info.clone());
    if let Err(error) = room.client().state_store().save_changes(&state_changes).await {
        tracing::warn!(
            room_id = room_id.trim(),
            %error,
            "Failed to persist optimistic marked-unread update; \
             in-memory state is still applied, but the flag may \
             revert after restart"
        );
    }

    room.update_room_info(|_| (info, RoomInfoNotableUpdateReasons::UNREAD_MARKER)).await;
}

pub async fn mark_room_as_read(
    handle_id: u64,
    room_id: &str,
    public_receipt: bool,
) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;
    // UFCS to the synchronous matrix_sdk_base inherent (mirrors the call in
    // runtime_room_list.rs); avoids dispatching to the async UI extension trait.
    let latest_event: BaseLatestEventValue = matrix_sdk_base::Room::latest_event(&room);
    let event_id = latest_event.event_id().map(|id| id.to_string()).ok_or_else(|| {
        format!(
            "matrix-sdk room {} has no known latest event to anchor a read receipt on",
            room_id.trim()
        )
    })?;

    mark_room_event_as_read(handle_id, room_id, &event_id, public_receipt).await
}

pub async fn mark_room_unread(handle_id: u64, room_id: &str, unread: bool) -> Result<(), String> {
    let room = joined_room_for_handle(handle_id, room_id)?;

    tracing::info!(
        handle_id,
        room_id = room_id.trim(),
        unread,
        "Setting matrix-sdk room marked-unread flag"
    );

    room.set_unread_flag(unread)
        .await
        .map_err(|e| format!("failed to set marked-unread flag: {e}"))?;

    // matrix-sdk's `set_unread_flag` only POSTs the account-data write; local
    // `RoomInfo.is_marked_unread` updates only when the server echoes via the
    // next sliding-sync round.  Mirror the value locally so the UI refreshes
    // immediately, matching the optimistic pattern in `mark_room_event_as_read`.
    optimistically_flip_marked_unread(&room, room_id, unread).await;

    Ok(())
}
