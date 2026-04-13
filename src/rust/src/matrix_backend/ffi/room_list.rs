// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn into_ffi_matrix_room_summary(
    room: matrix_backend::runtime::MatrixRoomSummary,
) -> ffi::MatrixRoomSummary {
    ffi::MatrixRoomSummary {
        room_id: room.room_id,
        latest_event_id: room.latest_event_id,
        display_name: room.display_name,
        avatar_url: room.avatar_url,
        topic: room.topic,
        room_alias: room.room_alias,
        last_message: room.last_message,
        last_message_kind: room.last_message_kind,
        last_message_sender_id: room.last_message_sender_id,
        last_message_sender_display_name: room.last_message_sender_display_name,
        tags: room.tags,
        parent_space_room_ids: room.parent_space_room_ids,
        direct_chat_other_user_id: room.direct_chat_other_user_id,
        is_invite: room.is_invite,
        inviter_user_id: room.inviter_user_id,
        inviter_display_name: room.inviter_display_name,
        inviter_avatar_url: room.inviter_avatar_url,
        invite_reason: room.invite_reason,
        is_space: room.is_space,
        is_direct: room.is_direct,
        is_bot_room: room.is_bot_room,
        is_encrypted: room.is_encrypted,
        is_public: room.is_public,
        member_count: room.member_count,
        unread_message_count: room.unread_message_count,
        notification_count: room.notification_count,
        highlight_count: room.highlight_count,
        timestamp: room.timestamp,
    }
}

pub(crate) fn into_ffi_matrix_notification_item(
    item: matrix_backend::runtime::MatrixNotificationItem,
) -> ffi::MatrixNotificationItem {
    ffi::MatrixNotificationItem {
        room_id: item.room_id,
        event_id: item.event_id,
        replacement_event_id: item.replacement_event_id,
        room_name: item.room_name,
        avatar_url: item.avatar_url,
        sender_display_name: item.sender_display_name,
        notification_kind: item.notification_kind,
        plain_body: item.plain_body,
        formatted_body: item.formatted_body,
        media_mxc_url: item.media_mxc_url,
        is_reply: item.is_reply,
        is_emote: item.is_emote,
        is_encrypted: item.is_encrypted,
        contains_spoiler: item.contains_spoiler,
        has_inline_image: item.has_inline_image,
        play_sound: item.play_sound,
    }
}

pub(crate) fn into_ffi_matrix_turn_server_info(
    info: matrix_backend::runtime::MatrixTurnServerInfo,
) -> ffi::MatrixTurnServerInfo {
    ffi::MatrixTurnServerInfo {
        username: info.username,
        password: info.password,
        uris: info.uris,
        ttl_seconds: info.ttl_seconds,
    }
}

pub(crate) fn into_ffi_matrix_image_pack_image(
    image: matrix_backend::runtime::MatrixImagePackImage,
) -> ffi::MatrixImagePackImage {
    ffi::MatrixImagePackImage {
        shortcode: image.shortcode,
        body: image.body,
        url: image.url,
        is_emote: image.is_emote,
        is_sticker: image.is_sticker,
    }
}

pub(crate) fn into_ffi_matrix_image_pack(
    pack: matrix_backend::runtime::MatrixImagePack,
) -> ffi::MatrixImagePack {
    ffi::MatrixImagePack {
        source_room_id: pack.source_room_id,
        state_key: pack.state_key,
        display_name: pack.display_name,
        avatar_url: pack.avatar_url,
        attribution: pack.attribution,
        is_emote_pack: pack.is_emote_pack,
        is_sticker_pack: pack.is_sticker_pack,
        from_space: pack.from_space,
        is_globally_enabled: pack.is_globally_enabled,
        images: pack
            .images
            .into_iter()
            .map(into_ffi_matrix_image_pack_image)
            .collect(),
    }
}

pub(crate) fn from_ffi_matrix_image_pack_image(
    image: ffi::MatrixImagePackImage,
) -> matrix_backend::runtime::MatrixImagePackImage {
    matrix_backend::runtime::MatrixImagePackImage {
        shortcode: image.shortcode,
        body: image.body,
        url: image.url,
        is_emote: image.is_emote,
        is_sticker: image.is_sticker,
    }
}

pub(crate) fn from_ffi_matrix_image_pack(
    pack: ffi::MatrixImagePack,
) -> matrix_backend::runtime::MatrixImagePack {
    matrix_backend::runtime::MatrixImagePack {
        source_room_id: pack.source_room_id,
        state_key: pack.state_key,
        display_name: pack.display_name,
        avatar_url: pack.avatar_url,
        attribution: pack.attribution,
        is_emote_pack: pack.is_emote_pack,
        is_sticker_pack: pack.is_sticker_pack,
        from_space: pack.from_space,
        is_globally_enabled: pack.is_globally_enabled,
        images: pack
            .images
            .into_iter()
            .map(from_ffi_matrix_image_pack_image)
            .collect(),
    }
}

pub(crate) fn matrix_fetch_room_list(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<Vec<ffi::MatrixRoomSummary>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_room_list",
        matrix_backend::runtime::fetch_room_list(handle_id),
    )
    .map(|rooms| rooms.into_iter().map(into_ffi_matrix_room_summary).collect())
}

pub(crate) fn matrix_fetch_notification_items(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    requests: &Vec<ffi::MatrixNotificationRequest>,
) -> Result<Vec<ffi::MatrixNotificationItem>, String> {
    let requests = requests
        .iter()
        .map(|request| matrix_backend::runtime::MatrixNotificationRequest {
            room_id: request.room_id.clone(),
            event_id: request.event_id.clone(),
        })
        .collect::<Vec<_>>();

    ffi_block_on(
        context,
        "matrix_fetch_notification_items",
        matrix_backend::runtime::fetch_notification_items(handle_id, &requests),
    )
    .map(|items| items.into_iter().map(into_ffi_matrix_notification_item).collect())
}

pub(crate) fn matrix_fetch_account_notifications_enabled(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<bool, String> {
    ffi_block_on(
        context,
        "matrix_fetch_account_notifications_enabled",
        matrix_backend::runtime::fetch_account_notifications_enabled(handle_id),
    )
}

pub(crate) fn matrix_fetch_turn_server_info(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixTurnServerInfo, String> {
    ffi_block_on(
        context,
        "matrix_fetch_turn_server_info",
        matrix_backend::runtime::fetch_turn_server_info(handle_id),
    )
    .map(into_ffi_matrix_turn_server_info)
}

pub(crate) fn matrix_set_account_notifications_enabled(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    enabled: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_account_notifications_enabled",
        matrix_backend::runtime::set_account_notifications_enabled(handle_id, enabled),
    )
}

pub(crate) fn matrix_fetch_image_packs(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<Vec<ffi::MatrixImagePack>, String> {
    ffi_block_on(
        context,
        "matrix_fetch_image_packs",
        matrix_backend::runtime::fetch_image_packs(handle_id, room_id),
    )
    .map(|packs| packs.into_iter().map(into_ffi_matrix_image_pack).collect())
}

pub(crate) fn matrix_save_image_pack(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    state_key: &str,
    previous_state_key: &str,
    has_previous_state_key: bool,
    pack: ffi::MatrixImagePack,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_save_image_pack",
        matrix_backend::runtime::save_image_pack(
            handle_id,
            room_id,
            state_key,
            previous_state_key,
            has_previous_state_key,
            from_ffi_matrix_image_pack(pack),
        ),
    )
}

pub(crate) fn matrix_remove_image_pack(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    state_key: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_remove_image_pack",
        matrix_backend::runtime::remove_image_pack(handle_id, room_id, state_key),
    )
}

pub(crate) fn matrix_set_image_pack_globally_enabled(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    state_key: &str,
    enabled: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_image_pack_globally_enabled",
        matrix_backend::runtime::set_image_pack_globally_enabled(
            handle_id, room_id, state_key, enabled,
        ),
    )
}
