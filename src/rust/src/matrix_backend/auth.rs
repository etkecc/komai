// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::{Client, ClientBuildError};

use super::bootstrap;

pub struct MatrixLoginResult {
    pub user_id: String,
    pub access_token: String,
    pub device_id: String,
    pub homeserver_url: String,
}

pub async fn login_password(
    profile_id: &str,
    homeserver_url: &str,
    user_id: &str,
    password: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<MatrixLoginResult, String> {
    let client = build_login_client(homeserver_url, verify_certificates)
        .await
        .map_err(|e| format!("failed to build matrix-sdk login client: {e}"))?;

    let mut login_builder = client.matrix_auth().login_username(user_id, password);
    if !device_id.trim().is_empty() {
        login_builder = login_builder.device_id(device_id);
    }
    if !initial_device_display_name.trim().is_empty() {
        login_builder = login_builder.initial_device_display_name(initial_device_display_name);
    }

    let response = login_builder
        .send()
        .await
        .map_err(|e| format!("failed to log in with password: {e}"))?;

    persist_login(
        profile_id,
        &client,
        response.user_id.to_string(),
        response.access_token,
        response.device_id.to_string(),
    )
}

pub async fn login_token(
    profile_id: &str,
    homeserver_url: &str,
    login_token: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<MatrixLoginResult, String> {
    let client = build_login_client(homeserver_url, verify_certificates)
        .await
        .map_err(|e| format!("failed to build matrix-sdk login client: {e}"))?;

    let mut login_builder = client.matrix_auth().login_token(login_token);
    if !device_id.trim().is_empty() {
        login_builder = login_builder.device_id(device_id);
    }
    if !initial_device_display_name.trim().is_empty() {
        login_builder = login_builder.initial_device_display_name(initial_device_display_name);
    }

    let response = login_builder
        .send()
        .await
        .map_err(|e| format!("failed to log in with token: {e}"))?;

    persist_login(
        profile_id,
        &client,
        response.user_id.to_string(),
        response.access_token,
        response.device_id.to_string(),
    )
}

async fn build_login_client(
    homeserver_url: &str,
    verify_certificates: bool,
) -> Result<Client, ClientBuildError> {
    let mut builder = Client::builder().homeserver_url(homeserver_url);
    if !verify_certificates {
        builder = builder.disable_ssl_verification();
    }
    builder.build().await
}

fn persist_login(
    profile_id: &str,
    client: &Client,
    user_id: String,
    access_token: String,
    device_id: String,
) -> Result<MatrixLoginResult, String> {
    let homeserver_url = client.homeserver().to_string();
    let store_passphrase = bootstrap::ensure_store_passphrase(profile_id);
    bootstrap::persist_current_session(profile_id, &store_passphrase, &homeserver_url, client)?;

    Ok(MatrixLoginResult {
        user_id,
        access_token,
        device_id,
        homeserver_url,
    })
}
