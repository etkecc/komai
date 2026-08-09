// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn matrix_fetch_recovery_status(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixRecoveryStatus, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_recovery_status",
        matrix_backend::runtime::fetch_recovery_status(handle_id),
    )?;

    Ok(ffi::MatrixRecoveryStatus {
        state: result.state,
        has_devices_to_verify_against: result.has_devices_to_verify_against,
        own_device_is_verified: result.own_device_is_verified,
        has_unverified_own_devices: result.has_unverified_own_devices,
    })
}

pub(crate) fn matrix_export_room_keys(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    path: &str,
    passphrase: &str,
) -> Result<u64, String> {
    ffi_block_on(
        context,
        "matrix_export_room_keys",
        matrix_backend::runtime::export_room_keys(handle_id, path, passphrase),
    )
}

pub(crate) fn matrix_import_room_keys(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    path: &str,
    passphrase: &str,
) -> Result<ffi::MatrixRoomKeyImportCounts, String> {
    let result = ffi_block_on(
        context,
        "matrix_import_room_keys",
        matrix_backend::runtime::import_room_keys(handle_id, path, passphrase),
    )?;

    Ok(ffi::MatrixRoomKeyImportCounts {
        imported: result.imported,
        total: result.total,
    })
}

pub(crate) fn matrix_setup_recovery(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    use_ssss: bool,
    passphrase: &str,
    encryption_backup_online_enabled: bool,
) -> Result<ffi::MatrixSetupRecoveryResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_setup_recovery",
        matrix_backend::runtime::setup_recovery(
            handle_id,
            use_ssss,
            passphrase,
            encryption_backup_online_enabled,
        ),
    )?;

    Ok(ffi::MatrixSetupRecoveryResult {
        recovery_key: result.recovery_key,
    })
}

pub(crate) fn matrix_recover_encryption_secrets(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    key_or_passphrase: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_recover_encryption_secrets",
        matrix_backend::runtime::recover_encryption_secrets(handle_id, key_or_passphrase),
    )
}

pub(crate) fn matrix_start_reset_encryption_identity(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixResetEncryptionIdentityResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_reset_encryption_identity",
        matrix_backend::runtime::start_reset_encryption_identity(handle_id),
    )?;

    Ok(ffi::MatrixResetEncryptionIdentityResult {
        completed: result.completed,
        auth_type: result.auth_type,
        approval_url: result.approval_url,
    })
}

pub(crate) fn matrix_continue_reset_encryption_identity_with_password(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    password: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_continue_reset_encryption_identity_with_password",
        matrix_backend::runtime::continue_reset_encryption_identity_with_password(
            handle_id, password,
        ),
    )
}

pub(crate) fn matrix_continue_reset_encryption_identity_after_approval(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_continue_reset_encryption_identity_after_approval",
        matrix_backend::runtime::continue_reset_encryption_identity_after_approval(handle_id),
    )
}

pub(crate) fn matrix_cancel_reset_encryption_identity(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_cancel_reset_encryption_identity",
        matrix_backend::runtime::cancel_reset_encryption_identity(handle_id),
    )
}

pub(crate) fn matrix_start_sign_out_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    device_id: &str,
) -> Result<ffi::MatrixDeviceSignOutResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_sign_out_device",
        matrix_backend::runtime::start_sign_out_device(handle_id, device_id),
    )?;

    Ok(ffi::MatrixDeviceSignOutResult {
        completed: result.completed,
        auth_type: result.auth_type,
        approval_url: result.approval_url,
    })
}

pub(crate) fn matrix_continue_sign_out_device_with_password(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    password: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_continue_sign_out_device_with_password",
        matrix_backend::runtime::continue_sign_out_device_with_password(handle_id, password),
    )
}

pub(crate) fn matrix_rename_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    device_id: &str,
    display_name: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_rename_device",
        matrix_backend::runtime::rename_device(handle_id, device_id, display_name),
    )
}
