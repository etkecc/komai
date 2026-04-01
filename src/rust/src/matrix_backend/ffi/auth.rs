// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, matrix_backend};

use super::blocking::ffi_block_on;

pub(crate) fn matrix_discover_login_flows(
    context: ffi::MatrixFfiBlockingContext,
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixLoginFlows, String> {
    let result = ffi_block_on(
        context,
        "matrix_discover_login_flows",
        matrix_backend::auth::discover_login_flows(server_name_or_url, verify_certificates),
    )?;

    Ok(ffi::MatrixLoginFlows {
        homeserver_url: result.homeserver_url,
        password_supported: result.password_supported,
        sso_supported: result.sso_supported,
        oauth_supported: result.oauth_supported,
        identity_providers: result
            .identity_providers
            .into_iter()
            .map(|provider| ffi::MatrixLoginIdentityProvider {
                id: provider.id,
                name: provider.name,
                icon: provider.icon,
                brand: provider.brand,
            })
            .collect(),
    })
}

pub(crate) fn matrix_get_sso_login_url(
    context: ffi::MatrixFfiBlockingContext,
    homeserver_url: &str,
    redirect_url: &str,
    identity_provider_id: &str,
    verify_certificates: bool,
) -> Result<String, String> {
    ffi_block_on(
        context,
        "matrix_get_sso_login_url",
        matrix_backend::auth::get_sso_login_url(
            homeserver_url,
            redirect_url,
            identity_provider_id,
            verify_certificates,
        ),
    )
}

pub(crate) fn matrix_start_sso_callback_server(
    success_html: &str,
    failure_html: &str,
    timeout_ms: u32,
) -> Result<ffi::MatrixSsoCallbackServer, String> {
    let result =
        matrix_backend::auth::start_sso_callback_server(success_html, failure_html, timeout_ms)?;

    Ok(ffi::MatrixSsoCallbackServer {
        listener_id: result.listener_id,
        callback_url: result.callback_url,
    })
}

pub(crate) fn matrix_poll_sso_callback_server(
    listener_id: u64,
) -> Result<ffi::MatrixSsoCallbackStatus, String> {
    let result = matrix_backend::auth::poll_sso_callback_server(listener_id)?;

    Ok(ffi::MatrixSsoCallbackStatus {
        ready: result.ready,
        success: result.success,
        login_token: result.login_token,
        callback_query: result.callback_query,
    })
}

pub(crate) fn matrix_stop_sso_callback_server(listener_id: u64) -> Result<(), String> {
    matrix_backend::auth::stop_sso_callback_server(listener_id)
}

pub(crate) fn matrix_start_oauth_login(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
    homeserver_url: &str,
    redirect_url: &str,
    user_id_hint: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixOauthLoginStartResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_start_oauth_login",
        matrix_backend::auth::start_oauth_login(
            profile_id,
            homeserver_url,
            redirect_url,
            user_id_hint,
            device_id,
            initial_device_display_name,
            verify_certificates,
        ),
    )?;

    Ok(ffi::MatrixOauthLoginStartResult {
        login_id: result.login_id,
        login_url: result.login_url,
    })
}

pub(crate) fn matrix_finish_oauth_login(
    context: ffi::MatrixFfiBlockingContext,
    login_id: u64,
    callback_query: &str,
) -> Result<ffi::MatrixLoginResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_finish_oauth_login",
        matrix_backend::auth::finish_oauth_login(login_id, callback_query),
    )?;

    Ok(ffi::MatrixLoginResult {
        user_id: result.user_id,
        access_token: result.access_token,
        device_id: result.device_id,
        homeserver_url: result.homeserver_url,
    })
}

pub(crate) fn matrix_cancel_oauth_login(login_id: u64) -> Result<(), String> {
    matrix_backend::auth::cancel_oauth_login(login_id)
}

pub(crate) fn matrix_login_password(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
    homeserver_url: &str,
    user_id: &str,
    password: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixLoginResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_login_password",
        matrix_backend::auth::login_password(
            profile_id,
            homeserver_url,
            user_id,
            password,
            device_id,
            initial_device_display_name,
            verify_certificates,
        ),
    )?;

    Ok(ffi::MatrixLoginResult {
        user_id: result.user_id,
        access_token: result.access_token,
        device_id: result.device_id,
        homeserver_url: result.homeserver_url,
    })
}

pub(crate) fn matrix_login_token(
    context: ffi::MatrixFfiBlockingContext,
    profile_id: &str,
    homeserver_url: &str,
    login_token: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<ffi::MatrixLoginResult, String> {
    let result = ffi_block_on(
        context,
        "matrix_login_token",
        matrix_backend::auth::login_token(
            profile_id,
            homeserver_url,
            login_token,
            device_id,
            initial_device_display_name,
            verify_certificates,
        ),
    )?;

    Ok(ffi::MatrixLoginResult {
        user_id: result.user_id,
        access_token: result.access_token,
        device_id: result.device_id,
        homeserver_url: result.homeserver_url,
    })
}
