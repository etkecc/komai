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

fn runtime_to_ffi_timeline_item(item: MatrixTimelineItem) -> ffi::MatrixTimelineItem {
    ffi::MatrixTimelineItem {
        item_id: item.item_id,
        event_id: item.event_id,
        delivery_state: item.delivery_state,
        thread_id: item.thread_id,
        is_thread_root: item.is_thread_root,
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
        state_event_detail: item.state_event_detail,
        state_event_reason: item.state_event_reason,
        state_event_has_sender: item.state_event_has_sender,
    }
}

pub(crate) fn matrix_select_active_room_timeline(handle_id: u64, room_id: &str) -> Result<(), String> {
    matrix_backend::runtime::select_active_room_timeline(handle_id, room_id)
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
