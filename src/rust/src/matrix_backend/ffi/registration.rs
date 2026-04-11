// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn matrix_registration_probe(
    context: ffi::MatrixFfiBlockingContext,
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<ffi::RegistrationProbeResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_registration_probe",
        matrix_backend::registration::probe_registration_flows(
            server_name_or_url,
            verify_certificates,
        ),
    )?;

    Ok(ffi::RegistrationProbeResult {
        registration_id: result.registration_id,
        homeserver_url: result.homeserver_url,
        session: result.session,
        chosen_flow_stages: result.chosen_flow_stages,
        all_flows: result
            .all_flows
            .into_iter()
            .map(|stages| ffi::RegistrationFlowStages { stages })
            .collect(),
        terms_policies: result
            .terms_policies
            .into_iter()
            .map(|p| ffi::RegistrationTermsPolicy {
                id: p.id,
                version: p.version,
                name: p.name,
                url: p.url,
            })
            .collect(),
    })
}

pub(crate) fn matrix_registration_check_username(
    context: ffi::MatrixFfiBlockingContext,
    registration_id: u64,
    username: &str,
) -> Result<ffi::RegistrationUsernameResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_registration_check_username",
        matrix_backend::registration::check_username_available(registration_id, username),
    )?;

    Ok(ffi::RegistrationUsernameResult {
        available: result.available,
    })
}

pub(crate) fn matrix_registration_submit_stage(
    context: ffi::MatrixFfiBlockingContext,
    registration_id: u64,
    username: &str,
    password: &str,
    device_name: &str,
    stage_type: &str,
    token: &str,
    email_sid: &str,
    email_client_secret: &str,
) -> Result<ffi::RegistrationSubmitResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_registration_submit_stage",
        matrix_backend::registration::submit_registration_stage(
            registration_id,
            username,
            password,
            device_name,
            stage_type,
            token,
            email_sid,
            email_client_secret,
        ),
    )?;

    Ok(ffi::RegistrationSubmitResult {
        completed: result.completed,
        user_id: result.user_id,
        access_token: result.access_token,
        device_id: result.device_id,
        homeserver_url: result.homeserver_url,
        session: result.session,
        remaining_stages: result.remaining_stages,
        completed_stages: result.completed_stages,
        terms_policies: result
            .terms_policies
            .into_iter()
            .map(|p| ffi::RegistrationTermsPolicy {
                id: p.id,
                version: p.version,
                name: p.name,
                url: p.url,
            })
            .collect(),
    })
}

pub(crate) fn matrix_registration_request_email_token(
    context: ffi::MatrixFfiBlockingContext,
    registration_id: u64,
    email: &str,
    client_secret: &str,
    send_attempt: u64,
) -> Result<ffi::RegistrationEmailTokenResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_registration_request_email_token",
        matrix_backend::registration::request_email_token(
            registration_id,
            email,
            client_secret,
            send_attempt,
        ),
    )?;

    Ok(ffi::RegistrationEmailTokenResult {
        sid: result.sid,
    })
}

pub(crate) fn matrix_registration_cancel(registration_id: u64) -> Result<(), String> {
    matrix_backend::registration::cancel_registration(registration_id)
}
