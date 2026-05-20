// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Homeserver discovery, password + token login flows, and persisting a
//! successful login to disk.

use super::*;
use super::errors::{format_client_build_error, format_http_error};


pub async fn discover_login_flows(
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<MatrixLoginFlows, String> {
    tracing::info!(
        server_name_or_url,
        verify_certificates,
        "Discovering Matrix login flows"
    );

    let client = build_discovery_client(server_name_or_url, verify_certificates)
        .await
        .map_err(|e| format_client_build_error(&e))?;

    let flows = client
        .matrix_auth()
        .get_login_types()
        .await
        .map_err(|e| format_http_error("Failed to contact the homeserver", &e))?;

    let mut identity_providers = Vec::new();
    let mut sso_supported = false;
    let mut password_supported = false;

    for flow in flows.flows {
        match flow {
            LoginType::Password(_) => password_supported = true,
            LoginType::Sso(sso) => {
                sso_supported = true;

                for idp in sso.identity_providers {
                    identity_providers.push(MatrixLoginIdentityProvider {
                        id: idp.id,
                        name: idp.name,
                        icon: idp.icon.map(|icon| icon.to_string()).unwrap_or_default(),
                        brand: idp.brand.map(|brand| brand.to_string()).unwrap_or_default(),
                    });
                }
            }
            _ => {}
        }
    }

    if !password_supported && !sso_supported {
        password_supported = true;
    }

    let oauth_supported = match client.oauth().server_metadata().await {
        Ok(_) => true,
        Err(error) => {
            tracing::debug!(
                homeserver_url = %client.homeserver(),
                error = %error,
                "OAuth login is unavailable for this homeserver"
            );
            false
        }
    };

    if oauth_supported {
        sso_supported = true;
    }

    tracing::info!(
        homeserver_url = %client.homeserver(),
        password_supported,
        sso_supported,
        oauth_supported,
        identity_provider_count = identity_providers.len(),
        "Discovered Matrix login flows"
    );

    Ok(MatrixLoginFlows {
        homeserver_url: client.homeserver().to_string(),
        password_supported,
        sso_supported,
        oauth_supported,
        identity_providers,
    })
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
    tracing::info!(
        profile_id,
        homeserver_url,
        user_id = %user_id,
        reusing_device_id = !device_id.trim().is_empty(),
        verify_certificates,
        "Logging in with Matrix password flow"
    );

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
    tracing::info!(
        profile_id,
        homeserver_url,
        reusing_device_id = !device_id.trim().is_empty(),
        verify_certificates,
        "Logging in with Matrix token flow"
    );

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

pub(super) async fn build_login_client(
    homeserver_url: &str,
    verify_certificates: bool,
) -> Result<Client, ClientBuildError> {
    let mut builder = Client::builder().homeserver_url(homeserver_url);
    if !verify_certificates {
        builder = builder.disable_ssl_verification();
    }
    builder.build().await
}

pub(super) async fn build_discovery_client(
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<Client, ClientBuildError> {
    // Discovery is interactive: the user is staring at "Checking server...".
    // matrix-sdk's default RequestConfig has no retry limit and treats any 5xx
    // as transient, retrying with exponential backoff for up to 15 minutes
    // (e.g. when a homeserver returns 500 on /_matrix/client/v1/auth_metadata).
    // Fail fast instead and let the user retry by clicking Continue again.
    let mut builder = Client::builder()
        .server_name_or_homeserver_url(server_name_or_url)
        .request_config(RequestConfig::default().disable_retry());
    if !verify_certificates {
        builder = builder.disable_ssl_verification();
    }
    builder.build().await
}

pub(super) fn persist_login(
    profile_id: &str,
    client: &Client,
    user_id: String,
    access_token: String,
    device_id: String,
) -> Result<MatrixLoginResult, String> {
    let homeserver_url = client.homeserver().to_string();
    let store_passphrase = bootstrap::ensure_store_passphrase(profile_id);
    bootstrap::persist_current_session(profile_id, &store_passphrase, &homeserver_url, client)?;

    tracing::info!(
        profile_id,
        homeserver_url = %homeserver_url,
        user_id = %user_id,
        device_id = %device_id,
        "Persisted Matrix login session"
    );

    Ok(MatrixLoginResult {
        user_id,
        access_token,
        device_id,
        homeserver_url,
    })
}
