// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn matrix_start_self_verification(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_self_verification",
        matrix_backend::runtime::start_self_verification(handle_id),
    )?;

    Ok(ffi::MatrixVerificationSession {
        flow_id: result.flow_id,
        user_id: result.user_id,
        device_id: result.device_id,
        state: result.state,
        error: result.error,
        sender: result.sender,
        is_self_verification: result.is_self_verification,
        is_multi_device_verification: result.is_multi_device_verification,
        sas_numbers: result.sas_numbers,
    })
}

pub(crate) fn matrix_start_user_verification(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_user_verification",
        matrix_backend::runtime::start_user_verification(handle_id, user_id),
    )?;

    Ok(ffi::MatrixVerificationSession {
        flow_id: result.flow_id,
        user_id: result.user_id,
        device_id: result.device_id,
        state: result.state,
        error: result.error,
        sender: result.sender,
        is_self_verification: result.is_self_verification,
        is_multi_device_verification: result.is_multi_device_verification,
        sas_numbers: result.sas_numbers,
    })
}

pub(crate) fn matrix_start_device_verification(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
    device_id: &str,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_device_verification",
        matrix_backend::runtime::start_device_verification(handle_id, user_id, device_id),
    )?;

    Ok(ffi::MatrixVerificationSession {
        flow_id: result.flow_id,
        user_id: result.user_id,
        device_id: result.device_id,
        state: result.state,
        error: result.error,
        sender: result.sender,
        is_self_verification: result.is_self_verification,
        is_multi_device_verification: result.is_multi_device_verification,
        sas_numbers: result.sas_numbers,
    })
}

pub(crate) fn matrix_unverify_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
    device_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unverify_device",
        matrix_backend::runtime::unverify_device(handle_id, user_id, device_id),
    )
}

pub(crate) fn matrix_block_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
    device_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_block_device",
        matrix_backend::runtime::block_device(handle_id, user_id, device_id),
    )
}

pub(crate) fn matrix_unblock_device(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
    device_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_unblock_device",
        matrix_backend::runtime::unblock_device(handle_id, user_id, device_id),
    )
}

pub(crate) fn matrix_fetch_user_verification_state(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    user_id: &str,
) -> Result<ffi::MatrixUserVerificationState, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_user_verification_state",
        matrix_backend::runtime::fetch_user_verification_state(handle_id, user_id),
    )?;

    Ok(ffi::MatrixUserVerificationState {
        has_master_key: result.has_master_key,
        user_trust: result.user_trust,
        devices: result
            .devices
            .into_iter()
            .map(|device| ffi::MatrixUserDevice {
                device_id: device.device_id,
                display_name: device.display_name,
                verification_state: device.verification_state,
                last_seen_ip: device.last_seen_ip,
                last_seen_ts: device.last_seen_ts,
            })
            .collect(),
    })
}

pub(crate) fn matrix_take_pending_verification_flow_ids(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
) -> Result<Vec<String>, String> {
    Ok(ffi_block_on(
        context,
        "matrix_take_pending_verification_flow_ids",
        async move { matrix_backend::runtime::take_pending_verification_flow_ids(handle_id) },
    )?)
}

pub(crate) fn matrix_fetch_verification_session(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    flow_id: &str,
) -> Result<ffi::MatrixVerificationSession, String> {
    let result = ffi_block_on(
        context,
        "matrix_fetch_verification_session",
        matrix_backend::runtime::fetch_verification_session(handle_id, flow_id),
    )?;

    Ok(ffi::MatrixVerificationSession {
        flow_id: result.flow_id,
        user_id: result.user_id,
        device_id: result.device_id,
        state: result.state,
        error: result.error,
        sender: result.sender,
        is_self_verification: result.is_self_verification,
        is_multi_device_verification: result.is_multi_device_verification,
        sas_numbers: result.sas_numbers,
    })
}

pub(crate) fn matrix_clear_verification_session(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    flow_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_clear_verification_session",
        matrix_backend::runtime::clear_verification_session(handle_id, flow_id),
    )
}

pub(crate) fn matrix_advance_verification_session(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    flow_id: &str,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_advance_verification_session",
        matrix_backend::runtime::advance_verification_session(handle_id, flow_id),
    )
}

pub(crate) fn matrix_cancel_verification_session(
    context: ffi::MatrixFfiBlockingContext,
    handle_id: u64,
    flow_id: &str,
    mismatch: bool,
) -> Result<(), String> {
    ffi_block_on(
        context,
        "matrix_cancel_verification_session",
        matrix_backend::runtime::cancel_verification_session(handle_id, flow_id, mismatch),
    )
}
