// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};
use matrix_backend::runtime::MatrixTimelineItem;

use super::blocking::ffi_block_on;

fn runtime_to_ffi_timeline_items(
    items: Vec<MatrixTimelineItem>,
) -> Vec<ffi::MatrixTimelineItem> {
    items
        .into_iter()
        .map(runtime_to_ffi_timeline_item)
        .collect()
}

pub(super) fn runtime_to_ffi_timeline_item(item: MatrixTimelineItem) -> ffi::MatrixTimelineItem {
    ffi::MatrixTimelineItem {
        item_id: item.item_id,
        event_id: item.event_id,
        transaction_id: item.transaction_id,
        delivery_state: item.delivery_state,
        send_error: item.send_error,
        is_recoverable: item.is_recoverable,
        thread_id: item.thread_id,
        is_thread_root: item.is_thread_root,
        thread_reply_count: item.thread_reply_count,
        sender_id: item.sender_id,
        sender_display_name: item.sender_display_name,
        sender_avatar_url: item.sender_avatar_url,
        body: item.body,
        formatted_body: item.formatted_body,
        reply_event_id: item.reply_event_id,
        reply_sender_id: item.reply_sender_id,
        reply_sender_display_name: item.reply_sender_display_name,
        reply_item_kind: item.reply_item_kind,
        reply_matrix_event_type: item.reply_matrix_event_type,
        reply_body: item.reply_body,
        reply_formatted_body: item.reply_formatted_body,
        reply_media_url: item.reply_media_url,
        reply_thumbnail_url: item.reply_thumbnail_url,
        reply_file_name: item.reply_file_name,
        reply_mime_type: item.reply_mime_type,
        reply_media_width: item.reply_media_width,
        reply_media_height: item.reply_media_height,
        reply_media_duration_ms: item.reply_media_duration_ms,
        reply_media_size_bytes: item.reply_media_size_bytes,
        reply_blurhash: item.reply_blurhash,
        reactions: item
            .reactions
            .into_iter()
            .map(|reaction| ffi::MatrixReactionSummary {
                key: reaction.key,
                users: reaction.users,
                user_ids: reaction.user_ids,
                self_reacted_event: reaction.self_reacted_event,
                count: reaction.count,
            })
            .collect(),
        reactions_summary: item.reactions_summary,
        special_effect_names: item.special_effect_names,
        item_kind: item.item_kind,
        membership_change_kind: item.membership_change_kind,
        matrix_event_type: item.matrix_event_type,
        is_edited: item.is_edited,
        media_url: item.media_url,
        thumbnail_url: item.thumbnail_url,
        file_name: item.file_name,
        mime_type: item.mime_type,
        media_width: item.media_width,
        media_height: item.media_height,
        media_duration_ms: item.media_duration_ms,
        media_size_bytes: item.media_size_bytes,
        blurhash: item.blurhash,
        media_is_encrypted: item.media_is_encrypted,
        thumbnail_is_encrypted: item.thumbnail_is_encrypted,
        is_voice_message: item.is_voice_message,
        waveform: item.waveform,
        timestamp: item.timestamp,
        is_own: item.is_own,
        state_event_target_user: item.state_event_target_user,
        state_event_target_user_id: item.state_event_target_user_id,
        state_event_detail: item.state_event_detail,
        state_event_reason: item.state_event_reason,
        state_event_has_sender: item.state_event_has_sender,
        utd_cause: item.utd_cause,
        is_encrypted_event: item.is_encrypted_event,
        shield_color: item.shield_color,
        shield_code: item.shield_code,
        power_level_changes: item
            .power_level_changes
            .into_iter()
            .map(|change| ffi::MatrixPowerLevelChange {
                user_id: change.user_id,
                old_level: change.old_level,
                new_level: change.new_level,
            })
            .collect(),
        server_acl_allowed_added: item
            .server_acl_changes
            .as_ref()
            .map(|c| c.allowed_added.clone())
            .unwrap_or_default(),
        server_acl_allowed_removed: item
            .server_acl_changes
            .as_ref()
            .map(|c| c.allowed_removed.clone())
            .unwrap_or_default(),
        server_acl_denied_added: item
            .server_acl_changes
            .as_ref()
            .map(|c| c.denied_added.clone())
            .unwrap_or_default(),
        server_acl_denied_removed: item
            .server_acl_changes
            .as_ref()
            .map(|c| c.denied_removed.clone())
            .unwrap_or_default(),
        server_acl_ip_literals_change: match item.server_acl_changes.as_ref().and_then(|c| c.ip_literals_changed) {
            None => 0,
            Some(true) => 1,
            Some(false) => 2,
        },
        tombstone_replacement_room_id: item.tombstone_replacement_room_id,
    }
}

pub(crate) fn matrix_select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<(), String> {
    matrix_backend::runtime::select_active_room_timeline(handle_id, room_id)
}

pub(crate) fn matrix_subscribe_to_room(handle_id: u64, room_id: &str) -> Result<(), String> {
    matrix_backend::runtime::subscribe_room(handle_id, room_id)
}

pub(crate) fn matrix_unsubscribe_from_room(handle_id: u64, room_id: &str) -> Result<(), String> {
    matrix_backend::runtime::unsubscribe_room(handle_id, room_id)
}

pub(crate) fn matrix_set_active_room_timeline_initial_page_size(
    handle_id: u64,
    page_size: u16,
) -> Result<(), String> {
    matrix_backend::runtime::set_active_room_timeline_initial_page_size(handle_id, page_size)
}

pub(crate) fn matrix_fetch_active_room_timeline(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<Vec<ffi::MatrixTimelineItem>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_active_room_timeline",
        matrix_backend::runtime::fetch_active_room_timeline(handle_id),
    )
    .map(runtime_to_ffi_timeline_items)
}

pub(crate) fn matrix_fetch_room_timeline(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    limit: u16,
) -> Result<Vec<ffi::MatrixTimelineItem>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_room_timeline",
        matrix_backend::runtime::fetch_room_timeline(handle_id, room_id, limit),
    )
    .map(runtime_to_ffi_timeline_items)
}

pub(crate) fn matrix_stop_room_timeline(
    handle_id: u64,
    room_id: &str,
) -> Result<(), String> {
    matrix_backend::runtime::stop_room_timeline(handle_id, room_id)
}

pub(crate) fn matrix_fetch_room_timeline_snapshot(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<ffi::MatrixTimelineItem>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_room_timeline_snapshot",
        matrix_backend::runtime::fetch_room_timeline_snapshot(handle_id, room_id),
    )
    .map(runtime_to_ffi_timeline_items)
}

pub(crate) fn matrix_paginate_active_room_timeline_backwards(
    handle_id: u64,
    page_size: u16,
) -> Result<(), String> {
    matrix_backend::runtime::paginate_active_room_timeline_backwards(handle_id, page_size)
}

pub(crate) fn matrix_subscribe_to_thread_timeline(
    handle_id: u64,
    room_id: &str,
    thread_root_id: &str,
) -> Result<(), String> {
    matrix_backend::runtime::subscribe_to_thread_timeline(handle_id, room_id, thread_root_id)
}

pub(crate) fn matrix_unsubscribe_from_thread_timeline(handle_id: u64) -> Result<(), String> {
    matrix_backend::runtime::unsubscribe_from_thread_timeline(handle_id)
}

pub(crate) fn matrix_refresh_thread_timeline(handle_id: u64) -> Result<(), String> {
    matrix_backend::runtime::refresh_thread_timeline(handle_id)
}

pub(crate) fn matrix_fetch_thread_timeline_snapshot(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<Vec<ffi::MatrixTimelineItem>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_thread_timeline_snapshot",
        matrix_backend::runtime::fetch_thread_timeline_snapshot(handle_id),
    )
    .map(runtime_to_ffi_timeline_items)
}

pub(crate) fn matrix_paginate_thread_timeline_backwards(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    num_events: u16,
) -> Result<bool, String> {
    ffi_block_on(
        context,
        "matrix_paginate_thread_timeline_backwards",
        matrix_backend::runtime::paginate_thread_timeline_backwards(handle_id, num_events),
    )
}

pub(crate) fn matrix_fetch_active_room_timeline_media_content(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    item_id: &str,
    width: i32,
    height: i32,
    crop: bool,
) -> Result<Vec<u8>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_active_room_timeline_media_content",
        matrix_backend::runtime::fetch_active_room_timeline_media_content(
            handle_id, item_id, width, height, crop,
        ),
    )
}

pub(crate) fn matrix_fetch_active_room_timeline_media_content_with_progress(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    item_id: &str,
) -> Result<Vec<u8>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_active_room_timeline_media_content_with_progress",
        matrix_backend::runtime::fetch_active_room_timeline_media_content_with_progress(
            handle_id, item_id,
        ),
    )
}

pub(crate) fn matrix_active_timeline_media_download_progress(
    handle_id: u64,
    item_id: &str,
) -> ffi::MatrixMediaDownloadProgress {
    let (received_bytes, total_bytes) =
        matrix_backend::runtime::active_timeline_media_download_progress(handle_id, item_id);
    ffi::MatrixMediaDownloadProgress {
        received_bytes,
        total_bytes,
    }
}
