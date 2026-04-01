// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn matrix_fetch_own_profile(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixOwnProfile, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_own_profile",
        matrix_backend::runtime::fetch_own_profile(handle_id),
    )?;

    Ok(ffi::MatrixOwnProfile {
        display_name: result.display_name,
        avatar_url: result.avatar_url,
    })
}

pub(crate) fn matrix_fetch_own_presence(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixOwnPresence, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_own_presence",
        matrix_backend::runtime::fetch_own_presence(handle_id),
    )?;

    Ok(ffi::MatrixOwnPresence {
        state: result.state,
        status_message: result.status_message,
    })
}

pub(crate) fn matrix_set_own_display_name(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    display_name: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_own_display_name",
        matrix_backend::runtime::set_own_display_name(handle_id, display_name),
    )
}

pub(crate) fn matrix_set_own_presence(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    presence_state: &str,
    status_message: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_own_presence",
        matrix_backend::runtime::set_own_presence(handle_id, presence_state, status_message),
    )
}

pub(crate) fn matrix_set_own_room_display_name(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    display_name: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_own_room_display_name",
        matrix_backend::runtime::set_own_room_display_name(handle_id, room_id, display_name),
    )
}

pub(crate) fn matrix_upload_own_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    file_path: &str,
    mime_type: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_upload_own_avatar",
        matrix_backend::runtime::upload_own_avatar(handle_id, file_path, mime_type),
    )
}

pub(crate) fn matrix_remove_own_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_remove_own_avatar",
        matrix_backend::runtime::remove_own_avatar(handle_id),
    )
}

pub(crate) fn matrix_upload_own_room_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    file_path: &str,
    mime_type: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_upload_own_room_avatar",
        matrix_backend::runtime::upload_own_room_avatar(handle_id, room_id, file_path, mime_type),
    )
}

pub(crate) fn matrix_remove_own_room_avatar(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_remove_own_room_avatar",
        matrix_backend::runtime::remove_own_room_avatar(handle_id, room_id),
    )
}

pub(crate) fn matrix_ignore_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_ignore_user",
        matrix_backend::runtime::ignore_user(handle_id, user_id),
    )
}

pub(crate) fn matrix_unignore_user(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unignore_user",
        matrix_backend::runtime::unignore_user(handle_id, user_id),
    )
}

pub(crate) fn matrix_set_invite_permission(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    target: &str,
    block: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_set_invite_permission",
        matrix_backend::runtime::set_invite_permission(handle_id, target, block),
    )
}
