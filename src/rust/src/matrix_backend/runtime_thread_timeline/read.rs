// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Per-thread receipt snapshot — which messages the user has read inside
//! the thread, derived from the latest receipts on the server.

use std::collections::HashSet;

use matrix_sdk::Room;
use matrix_sdk::ruma::EventId;
use matrix_sdk::ruma::events::receipt::{ReceiptThread, ReceiptType};

#[derive(Default)]
pub(super) struct ThreadReadState {
    /// Highest `m.read` receipt timestamp seen across other members (any of
    /// Main / Unthreaded / Thread(root)); a cumulative "read at least this
    /// far" watermark.
    pub(super) max_receipt_ts: u64,
    /// Event IDs those receipts point directly at.
    pub(super) receipt_event_ids: HashSet<String>,
}

pub(super) async fn fetch_thread_read_state(room: &Room, thread_root: &EventId) -> ThreadReadState {
    let own_user_id = room.own_user_id().to_owned();
    let members = match room.members(matrix_sdk::RoomMemberships::ACTIVE).await {
        Ok(m) => m,
        Err(error) => {
            tracing::warn!(%error, "Failed to load room members for thread read receipts");
            return ThreadReadState::default();
        }
    };
    // Threaded receipts first; some clients (e.g. bots) only ever send
    // those for thread messages, so they'd be missed if we only looked at
    // Main / Unthreaded.
    let receipt_threads = [
        ReceiptThread::Thread(thread_root.to_owned()),
        ReceiptThread::Unthreaded,
        ReceiptThread::Main,
    ];
    let mut state = ThreadReadState::default();
    for member in members.iter() {
        if member.user_id() == own_user_id {
            continue;
        }
        for thread in &receipt_threads {
            if let Ok(Some((receipt_event_id, receipt))) = room
                .load_user_receipt(ReceiptType::Read, thread.clone(), member.user_id())
                .await
            {
                let ts = receipt.ts.map(|t| u64::from(t.0)).unwrap_or(0);
                state.max_receipt_ts = state.max_receipt_ts.max(ts);
                state.receipt_event_ids.insert(receipt_event_id.to_string());
            }
        }
    }
    state
}
