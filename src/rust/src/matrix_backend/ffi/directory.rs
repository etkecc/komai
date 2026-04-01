// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn matrix_fetch_user_profile(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<ffi::MatrixUserProfile, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_user_profile",
        matrix_backend::runtime::fetch_user_profile(handle_id, user_id),
    )?;

    Ok(ffi::MatrixUserProfile {
        display_name: result.display_name,
        avatar_url: result.avatar_url,
    })
}

pub(crate) fn matrix_fetch_room_member_profile(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    room_id: &str,
    user_id: &str,
) -> Result<ffi::MatrixUserProfile, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_room_member_profile",
        matrix_backend::runtime::fetch_room_member_profile(handle_id, room_id, user_id),
    )?;

    Ok(ffi::MatrixUserProfile {
        display_name: result.display_name,
        avatar_url: result.avatar_url,
    })
}

pub(crate) fn matrix_search_users(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    search_term: &str,
    limit: u64,
) -> Result<Vec<ffi::MatrixDirectoryUser>, String> {
    ffi_block_on(
        context,
        "matrix_search_users",
        matrix_backend::runtime::search_users(handle_id, search_term, limit),
    )
    .map(|users| {
        users
            .into_iter()
            .map(|user| ffi::MatrixDirectoryUser {
                display_name: user.display_name,
                user_id: user.user_id,
                avatar_url: user.avatar_url,
            })
            .collect()
    })
}

pub(crate) fn matrix_fetch_public_room_directory_page(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    search_term: &str,
    limit: u64,
    since: &str,
    server: &str,
) -> Result<ffi::MatrixPublicRoomDirectoryPage, String> {
    ffi_block_on(
        context,
        "matrix_fetch_public_room_directory_page",
        matrix_backend::runtime::fetch_public_room_directory_page(
            handle_id,
            search_term,
            limit,
            since,
            server,
        ),
    )
    .map(|page| ffi::MatrixPublicRoomDirectoryPage {
        rooms: page
            .rooms
            .into_iter()
            .map(|room| ffi::MatrixPublicRoomDirectoryEntry {
                room_id: room.room_id,
                room_server_name: room.room_server_name,
                display_name: room.display_name,
                avatar_url: room.avatar_url,
                topic: room.topic,
                canonical_alias: room.canonical_alias,
                member_count: room.member_count,
                is_world_readable: room.is_world_readable,
                is_space: room.is_space,
            })
            .collect(),
        next_batch: page.next_batch,
        total_room_count_estimate: page.total_room_count_estimate,
    })
}
