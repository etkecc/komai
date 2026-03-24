// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::{
    Client, ClientBuildError, Error as MatrixSdkError, HttpError, RumaApiError,
    ruma::api::{
        client::{
            error::ErrorBody,
            session::get_login_types::v3::LoginType,
        },
        error::FromHttpResponseError,
    },
};

use super::bootstrap;

pub struct MatrixLoginIdentityProvider {
    pub id: String,
    pub name: String,
    pub icon: String,
    pub brand: String,
}

pub struct MatrixLoginFlows {
    pub homeserver_url: String,
    pub password_supported: bool,
    pub sso_supported: bool,
    pub identity_providers: Vec<MatrixLoginIdentityProvider>,
}

pub struct MatrixLoginResult {
    pub user_id: String,
    pub access_token: String,
    pub device_id: String,
    pub homeserver_url: String,
}

pub async fn discover_login_flows(
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<MatrixLoginFlows, String> {
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

    Ok(MatrixLoginFlows {
        homeserver_url: client.homeserver().to_string(),
        password_supported,
        sso_supported,
        identity_providers,
    })
}

pub async fn get_sso_login_url(
    homeserver_url: &str,
    redirect_url: &str,
    identity_provider_id: &str,
    verify_certificates: bool,
) -> Result<String, String> {
    let client = build_login_client(homeserver_url, verify_certificates)
        .await
        .map_err(|e| format!("failed to build matrix-sdk SSO client: {e}"))?;

    client
        .matrix_auth()
        .get_sso_login_url(
            redirect_url,
            (!identity_provider_id.trim().is_empty()).then_some(identity_provider_id),
        )
        .await
        .map_err(|e| format_sdk_error("Failed to build SSO redirect URL", &e))
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

async fn build_discovery_client(
    server_name_or_url: &str,
    verify_certificates: bool,
) -> Result<Client, ClientBuildError> {
    let mut builder = Client::builder().server_name_or_homeserver_url(server_name_or_url);
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

fn format_client_build_error(error: &ClientBuildError) -> String {
    match error {
        ClientBuildError::InvalidServerName => {
            "Received malformed response. Make sure the homeserver domain is valid.".to_owned()
        }
        ClientBuildError::AutoDiscovery(FromHttpResponseError::Deserialization(_)) => {
            "Autodiscovery failed. Received malformed response.".to_owned()
        }
        ClientBuildError::AutoDiscovery(FromHttpResponseError::Server(error)) => {
            format_ruma_api_error("Autodiscovery failed while requesting .well-known", error)
        }
        ClientBuildError::AutoDiscovery(_) => {
            "Autodiscovery failed. Unknown error when requesting .well-known.".to_owned()
        }
        ClientBuildError::Http(error) => format_http_error("Failed to contact the homeserver", error),
        _ => format!("Failed to discover Matrix login flows: {error}"),
    }
}

fn format_sdk_error(prefix: &str, error: &MatrixSdkError) -> String {
    if let Some(error) = error.as_client_api_error() {
        return format_client_api_error(prefix, error);
    }

    match error {
        MatrixSdkError::Http(error) => format_http_error(prefix, error),
        _ => format!("{prefix}: {error}"),
    }
}

fn format_http_error(prefix: &str, error: &HttpError) -> String {
    if let Some(error) = error.as_client_api_error() {
        return format_client_api_error(prefix, error);
    }

    if let HttpError::Reqwest(error) = error {
        return format!("{prefix}: {error}");
    }

    format!("{prefix}: {error}")
}

fn format_ruma_api_error(prefix: &str, error: &RumaApiError) -> String {
    if let Some(error) = error.as_client_api_error() {
        return format_client_api_error(prefix, error);
    }

    format!("{prefix}: {error}")
}

fn format_client_api_error(prefix: &str, error: &matrix_sdk::ruma::api::client::Error) -> String {
    if error.status_code.as_u16() == 404 {
        return "The required endpoints were not found. Possibly not a Matrix server.".to_owned();
    }

    match &error.body {
        ErrorBody::Standard(body) if !body.message.is_empty() => {
            format!("{prefix}: {}", body.message)
        }
        _ => format!("{prefix}: HTTP {}", error.status_code),
    }
}
