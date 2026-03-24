// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::{
    collections::HashMap,
    io::{ErrorKind, Read, Write},
    net::{TcpListener, TcpStream},
    sync::{
        Arc, Mutex, OnceLock,
        atomic::{AtomicBool, AtomicU64, Ordering},
    },
    thread,
    time::{Duration, Instant},
};

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
use reqwest::Url;

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

pub struct MatrixSsoCallbackServer {
    pub listener_id: u64,
    pub callback_url: String,
}

pub struct MatrixSsoCallbackStatus {
    pub ready: bool,
    pub success: bool,
    pub login_token: String,
}

#[derive(Clone)]
struct SsoCallbackResult {
    success: bool,
    login_token: String,
}

struct SsoListenerEntry {
    result: Mutex<Option<SsoCallbackResult>>,
    stop_requested: AtomicBool,
    join_handle: Mutex<Option<thread::JoinHandle<()>>>,
}

static NEXT_SSO_LISTENER_ID: AtomicU64 = AtomicU64::new(1);

fn sso_listeners() -> &'static Mutex<HashMap<u64, Arc<SsoListenerEntry>>> {
    static LISTENERS: OnceLock<Mutex<HashMap<u64, Arc<SsoListenerEntry>>>> = OnceLock::new();
    LISTENERS.get_or_init(|| Mutex::new(HashMap::new()))
}

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

    tracing::info!(
        homeserver_url = %client.homeserver(),
        password_supported,
        sso_supported,
        identity_provider_count = identity_providers.len(),
        "Discovered Matrix login flows"
    );

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
    tracing::debug!(
        homeserver_url,
        redirect_url,
        identity_provider_id,
        verify_certificates,
        "Building Matrix SSO redirect URL"
    );

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

pub fn start_sso_callback_server(
    success_html: &str,
    failure_html: &str,
    timeout_ms: u32,
) -> Result<MatrixSsoCallbackServer, String> {
    let listener = TcpListener::bind(("127.0.0.1", 0))
        .map_err(|e| format!("failed to bind local SSO callback server: {e}"))?;
    listener
        .set_nonblocking(true)
        .map_err(|e| format!("failed to configure local SSO callback server: {e}"))?;

    let port = listener
        .local_addr()
        .map_err(|e| format!("failed to read local SSO callback address: {e}"))?
        .port();

    let listener_id = NEXT_SSO_LISTENER_ID.fetch_add(1, Ordering::Relaxed);
    let entry = Arc::new(SsoListenerEntry {
        result: Mutex::new(None),
        stop_requested: AtomicBool::new(false),
        join_handle: Mutex::new(None),
    });

    let thread_entry = Arc::clone(&entry);
    let success_html = success_html.to_owned();
    let failure_html = failure_html.to_owned();
    let handle = thread::spawn(move || {
        run_sso_callback_server(
            listener,
            thread_entry,
            success_html,
            failure_html,
            Duration::from_millis(u64::from(timeout_ms)),
        )
    });
    *entry.join_handle.lock().expect("poisoned SSO listener handle mutex") = Some(handle);

    sso_listeners()
        .lock()
        .expect("poisoned SSO listener registry mutex")
        .insert(listener_id, entry);

    tracing::info!(
        listener_id,
        port,
        timeout_ms,
        "Started local Matrix SSO callback listener"
    );

    Ok(MatrixSsoCallbackServer {
        listener_id,
        callback_url: format!("http://localhost:{port}/sso"),
    })
}

pub fn poll_sso_callback_server(listener_id: u64) -> Result<MatrixSsoCallbackStatus, String> {
    let (entry, result) = {
        let listeners = sso_listeners()
            .lock()
            .expect("poisoned SSO listener registry mutex");
        let Some(entry) = listeners.get(&listener_id).cloned() else {
            return Err(format!("unknown SSO callback listener: {listener_id}"));
        };

        let Some(result) = entry
            .result
            .lock()
            .expect("poisoned SSO listener result mutex")
            .clone()
        else {
            return Ok(MatrixSsoCallbackStatus {
                ready: false,
                success: false,
                login_token: String::new(),
            });
        };

        (entry, result)
    };

    sso_listeners()
        .lock()
        .expect("poisoned SSO listener registry mutex")
        .remove(&listener_id);
    join_sso_listener(&entry);

    tracing::info!(
        listener_id,
        success = result.success,
        has_login_token = !result.login_token.is_empty(),
        "SSO callback listener completed"
    );

    Ok(MatrixSsoCallbackStatus {
        ready: true,
        success: result.success,
        login_token: result.login_token,
    })
}

pub fn stop_sso_callback_server(listener_id: u64) -> Result<(), String> {
    let entry = sso_listeners()
        .lock()
        .expect("poisoned SSO listener registry mutex")
        .remove(&listener_id);

    let Some(entry) = entry else {
        tracing::debug!(listener_id, "SSO callback listener was already absent");
        return Ok(());
    };

    entry.stop_requested.store(true, Ordering::Relaxed);
    join_sso_listener(&entry);
    tracing::info!(listener_id, "Stopped local Matrix SSO callback listener");
    Ok(())
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

fn run_sso_callback_server(
    listener: TcpListener,
    entry: Arc<SsoListenerEntry>,
    success_html: String,
    failure_html: String,
    timeout: Duration,
) {
    let deadline = Instant::now() + timeout;

    loop {
        if entry.stop_requested.load(Ordering::Relaxed) {
            set_sso_callback_result(&entry, false, String::new());
            return;
        }

        if Instant::now() >= deadline {
            set_sso_callback_result(&entry, false, String::new());
            return;
        }

        match listener.accept() {
            Ok((stream, _)) => {
                let result = handle_sso_callback_request(stream, &success_html, &failure_html);
                set_sso_callback_result(&entry, result.success, result.login_token);
                return;
            }
            Err(error) if error.kind() == ErrorKind::WouldBlock => {
                thread::sleep(Duration::from_millis(25));
            }
            Err(_) => {
                set_sso_callback_result(&entry, false, String::new());
                return;
            }
        }
    }
}

fn handle_sso_callback_request(
    mut stream: TcpStream,
    success_html: &str,
    failure_html: &str,
) -> SsoCallbackResult {
    let _ = stream.set_read_timeout(Some(Duration::from_secs(5)));
    let request_target = read_request_target(&mut stream).unwrap_or_default();
    let login_token = extract_login_token(&request_target).unwrap_or_default();
    let success = !login_token.is_empty();

    let status = if success { "200 OK" } else { "400 Bad Request" };
    let body = if success { success_html } else { failure_html };
    let _ = write_html_response(&mut stream, status, body);

    SsoCallbackResult {
        success,
        login_token,
    }
}

fn read_request_target(stream: &mut TcpStream) -> Option<String> {
    let mut request = Vec::new();
    let mut chunk = [0_u8; 1024];

    loop {
        match stream.read(&mut chunk) {
            Ok(0) => break,
            Ok(read) => {
                request.extend_from_slice(&chunk[..read]);
                if request.windows(4).any(|window| window == b"\r\n\r\n") || request.len() >= 8192 {
                    break;
                }
            }
            Err(error) if error.kind() == ErrorKind::WouldBlock => continue,
            Err(_) => return None,
        }
    }

    let request = String::from_utf8_lossy(&request);
    let mut parts = request.lines().next()?.split_whitespace();
    if parts.next()? != "GET" {
        return None;
    }

    parts.next().map(ToOwned::to_owned)
}

fn extract_login_token(request_target: &str) -> Option<String> {
    let url = Url::parse(&format!("http://localhost{request_target}")).ok()?;
    if url.path() != "/sso" {
        return None;
    }

    url.query_pairs().find_map(|(key, value)| {
        (key == "loginToken" && !value.is_empty()).then(|| value.into_owned())
    })
}

fn write_html_response(stream: &mut TcpStream, status: &str, body: &str) -> Result<(), std::io::Error> {
    let response = format!(
        "HTTP/1.1 {status}\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: {}\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n{}",
        body.as_bytes().len(),
        body
    );
    stream.write_all(response.as_bytes())?;
    stream.flush()
}

fn set_sso_callback_result(entry: &Arc<SsoListenerEntry>, success: bool, login_token: String) {
    let mut result = entry
        .result
        .lock()
        .expect("poisoned SSO listener result mutex");
    if result.is_none() {
        *result = Some(SsoCallbackResult {
            success,
            login_token,
        });
    }
}

fn join_sso_listener(entry: &Arc<SsoListenerEntry>) {
    let handle = entry
        .join_handle
        .lock()
        .expect("poisoned SSO listener handle mutex")
        .take();
    if let Some(handle) = handle {
        let _ = handle.join();
    }
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
