// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
    pub oauth_supported: bool,
    pub identity_providers: Vec<MatrixLoginIdentityProvider>,
}

pub struct MatrixLoginResult {
    pub user_id: String,
    pub access_token: String,
    pub device_id: String,
    pub homeserver_url: String,
}

pub struct MatrixSsoCallbackServer {
    pub listener_id: u64,
    pub callback_url: String,
}

pub struct MatrixSsoCallbackStatus {
    pub ready: bool,
    pub success: bool,
    pub login_token: String,
    pub callback_query: String,
}

pub struct MatrixOauthLoginStartResult {
    pub login_id: u64,
    pub login_url: String,
}

// Re-export the runtime-scope items the original flat file pulled in via
// plain `use` statements, at `pub(super)` so submodules pick them up via
// `use super::*;` without duplicating the import block.
pub(super) use std::collections::HashMap;
pub(super) use std::io::{ErrorKind, Read, Write};
pub(super) use std::net::{TcpListener, TcpStream};
pub(super) use std::sync::{Arc, Mutex, OnceLock};
pub(super) use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
pub(super) use std::thread;
pub(super) use std::time::{Duration, Instant};
pub(super) use matrix_sdk::{
    authentication::oauth::{
        ClientRegistrationData,
        registration::{ApplicationType, ClientMetadata, Localized, OAuthGrantType},
    },
    config::RequestConfig,
    utils::UrlOrQuery,
    Client, ClientBuildError, Error as MatrixSdkError, HttpError, RumaApiError,
    ruma::{
        api::{
            client::session::get_login_types::v3::LoginType,
            error::{ErrorBody, FromHttpResponseError},
        },
        serde::Raw,
    },
};
pub(super) use reqwest::Url;

pub(super) use super::bootstrap;

mod errors;
mod login;
mod oauth;
mod sso;

pub use login::{discover_login_flows, login_password, login_token};
pub use oauth::{cancel_oauth_login, finish_oauth_login, start_oauth_login};
pub use sso::{
    get_sso_login_url, poll_sso_callback_server, start_sso_callback_server,
    stop_sso_callback_server,
};
