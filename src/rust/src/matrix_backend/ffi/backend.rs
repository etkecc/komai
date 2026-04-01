// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn matrix_restore_session_preview(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
) -> Result<ffi::MatrixRestorePreview, String> {
    let preview = ffi_block_on(
        context,
        "matrix_restore_session_preview",
        matrix_backend::bootstrap::restore_session_preview(profile_id),
    )?;

    Ok(ffi::MatrixRestorePreview {
        has_session: preview.has_session,
        session_source: preview.session_source,
        auth_type: preview.auth_type,
        homeserver_url: preview.homeserver_url,
        user_id: preview.user_id,
        device_id: preview.device_id,
        state_store_root: preview.state_store_root,
        cache_root: preview.cache_root,
    })
}

pub(crate) fn matrix_start_restored_backend(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
) -> Result<ffi::MatrixBackendHandleInfo, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_restored_backend",
        matrix_backend::runtime::start_restored_backend(profile_id),
    )?;

    Ok(ffi::MatrixBackendHandleInfo {
        handle_id: result.handle_id,
        has_session: result.has_session,
        auth_type: result.auth_type,
        homeserver_url: result.homeserver_url,
        user_id: result.user_id,
        device_id: result.device_id,
    })
}

pub(crate) fn matrix_logout_backend(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_logout_backend",
        matrix_backend::runtime::logout_backend(handle_id),
    )
}

pub(crate) fn matrix_stop_backend(handle_id: u64) -> Result<(), String> {
    matrix_backend::runtime::stop_backend(handle_id)
}

pub(crate) fn matrix_start_media_proxy(handle_id: u64) -> Result<u16, String> {
    matrix_backend::runtime::start_media_proxy(handle_id)
}

pub(crate) fn matrix_is_timeline_media_encrypted(handle_id: u64, item_id: &str) -> bool {
    matrix_backend::runtime::is_timeline_media_encrypted(handle_id, item_id)
}

pub(crate) fn matrix_register_timeline_media_proxy_url(
    handle_id: u64,
    item_id: &str,
    file_extension: &str,
) -> Result<String, String> {
    matrix_backend::runtime::register_timeline_media_proxy_url(handle_id, item_id, file_extension)
}

pub(crate) fn matrix_stop_media_proxy(handle_id: u64) {
    matrix_backend::runtime::stop_media_proxy(handle_id)
}

pub(crate) fn matrix_start_backend_sync(handle_id: u64) -> Result<(), String> {
    matrix_backend::runtime::start_sync(handle_id)
}
