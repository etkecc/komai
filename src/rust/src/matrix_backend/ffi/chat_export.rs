// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::active_timeline::runtime_to_ffi_timeline_item;
use super::blocking::ffi_block_on;

pub(crate) fn matrix_fetch_chat_export_batch(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    from_token: &str,
    limit: u32,
) -> Result<ffi::MatrixChatExportBatch, String> {
    let batch = ffi_block_on(
        context,
        "matrix_fetch_chat_export_batch",
        matrix_backend::runtime::fetch_chat_export_batch(handle_id, room_id, from_token, limit),
    )?;

    Ok(ffi::MatrixChatExportBatch {
        events: batch
            .events
            .into_iter()
            .map(|event| ffi::MatrixChatExportEvent {
                item: runtime_to_ffi_timeline_item(event.item),
                relation_kind: event.relation_kind,
                relates_to_event_id: event.relates_to_event_id,
                annotation_key: event.annotation_key,
            })
            .collect(),
        next_token: batch.next_token,
        reached_start: batch.reached_start,
    })
}
