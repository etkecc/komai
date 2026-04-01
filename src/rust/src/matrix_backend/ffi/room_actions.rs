// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn matrix_join_room(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id_or_alias: &str,
    via: &Vec<String>,
    reason: &str,
) -> ffi::MatrixJoinRoomResult {
    match ffi_block_on(
        context,
        "matrix_join_room",
        matrix_backend::runtime::join_room(handle_id, room_id_or_alias, via.as_slice(), reason),
    ) {
        Ok(room_id) => ffi::MatrixJoinRoomResult {
            ok: true,
            room_id,
            error: String::new(),
            matrix_errcode: String::new(),
        },
        Err((error, matrix_errcode)) => ffi::MatrixJoinRoomResult {
            ok: false,
            room_id: room_id_or_alias.to_owned(),
            error,
            matrix_errcode,
        },
    }
}

pub(crate) fn matrix_knock_room(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id_or_alias: &str,
    via: &Vec<String>,
    reason: &str,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_knock_room",
        matrix_backend::runtime::knock_room(handle_id, room_id_or_alias, via.as_slice(), reason),
    )
}

#[allow(clippy::too_many_arguments)]
pub(crate) fn matrix_create_room(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    name: &str,
    topic: &str,
    room_alias_localpart: &str,
    invite_user_ids: &Vec<String>,
    preset: &str,
    is_direct: bool,
    is_encrypted: bool,
    is_space: bool,
    is_public: bool,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_create_room",
        matrix_backend::runtime::create_room(
            handle_id,
            name,
            topic,
            room_alias_localpart,
            invite_user_ids.as_slice(),
            preset,
            is_direct,
            is_encrypted,
            is_space,
            is_public,
        ),
    )
}

pub(crate) fn matrix_leave_room(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_leave_room",
        matrix_backend::runtime::leave_room(handle_id, room_id, reason),
    )
}

pub(crate) fn matrix_toggle_room_tag(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    tag: &str,
    enabled: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_toggle_room_tag",
        matrix_backend::runtime::toggle_room_tag(handle_id, room_id, tag, enabled),
    )
}

pub(crate) fn matrix_set_room_is_direct(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    is_direct: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_room_is_direct",
        matrix_backend::runtime::set_room_is_direct(handle_id, room_id, is_direct),
    )
}

pub(crate) fn matrix_invite_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_invite_user",
        matrix_backend::runtime::invite_user(handle_id, room_id, user_id, reason),
    )
}

pub(crate) fn matrix_kick_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_kick_user",
        matrix_backend::runtime::kick_user(handle_id, room_id, user_id, reason),
    )
}

pub(crate) fn matrix_ban_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_ban_user",
        matrix_backend::runtime::ban_user(handle_id, room_id, user_id, reason),
    )
}

pub(crate) fn matrix_unban_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
    reason: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unban_user",
        matrix_backend::runtime::unban_user(handle_id, room_id, user_id, reason),
    )
}
