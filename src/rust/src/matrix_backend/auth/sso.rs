// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! SSO callback flow: spins up a tiny localhost HTTP server, hands the
//! user out to the homeserver-provided SSO URL, and surfaces the login
//! token (or error) the homeserver redirects back with.

use super::*;
use super::errors::format_sdk_error;
use super::login::build_login_client;

#[derive(Clone)]
pub(super) struct SsoCallbackResult {
    success: bool,
    login_token: String,
    callback_query: String,
}

pub(super) struct SsoListenerEntry {
    result: Mutex<Option<SsoCallbackResult>>,
    stop_requested: AtomicBool,
    join_handle: Mutex<Option<thread::JoinHandle<()>>>,
}

pub(super) static NEXT_SSO_LISTENER_ID: AtomicU64 = AtomicU64::new(1);

pub(super) fn sso_listeners() -> &'static Mutex<HashMap<u64, Arc<SsoListenerEntry>>> {
    static LISTENERS: OnceLock<Mutex<HashMap<u64, Arc<SsoListenerEntry>>>> = OnceLock::new();
    LISTENERS.get_or_init(|| Mutex::new(HashMap::new()))
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
                callback_query: String::new(),
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
        callback_query: result.callback_query,
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

pub(super) fn run_sso_callback_server(
    listener: TcpListener,
    entry: Arc<SsoListenerEntry>,
    success_html: String,
    failure_html: String,
    timeout: Duration,
) {
    let deadline = Instant::now() + timeout;

    loop {
        if entry.stop_requested.load(Ordering::Relaxed) {
            set_sso_callback_result(&entry, false, String::new(), String::new());
            return;
        }

        if Instant::now() >= deadline {
            set_sso_callback_result(&entry, false, String::new(), String::new());
            return;
        }

        match listener.accept() {
            Ok((stream, _)) => {
                let result = handle_sso_callback_request(stream, &success_html, &failure_html);
                set_sso_callback_result(
                    &entry,
                    result.success,
                    result.login_token,
                    result.callback_query,
                );
                return;
            }
            Err(error) if error.kind() == ErrorKind::WouldBlock => {
                thread::sleep(Duration::from_millis(25));
            }
            Err(_) => {
                set_sso_callback_result(&entry, false, String::new(), String::new());
                return;
            }
        }
    }
}

pub(super) fn handle_sso_callback_request(
    mut stream: TcpStream,
    success_html: &str,
    failure_html: &str,
) -> SsoCallbackResult {
    let _ = stream.set_read_timeout(Some(Duration::from_secs(5)));
    let request_target = read_request_target(&mut stream).unwrap_or_default();
    let callback_query = extract_callback_query(&request_target).unwrap_or_default();
    let login_token = extract_login_token(&request_target).unwrap_or_default();
    let callback_error = extract_callback_error(&callback_query);
    let success = !callback_query.is_empty() && callback_error.is_none();

    let (status, body);
    if success {
        status = "200 OK";
        body = success_html.to_owned();
    } else {
        status = "400 Bad Request";
        let error_code = callback_error.as_deref().unwrap_or("unknown");
        let error_code_escaped = html_escape(error_code);
        body = failure_html.replace("{{SSO_ERROR_CODE}}", &error_code_escaped);
    }
    let _ = write_html_response(&mut stream, status, &body);

    SsoCallbackResult {
        success,
        login_token,
        callback_query,
    }
}

pub(super) fn read_request_target(stream: &mut TcpStream) -> Option<String> {
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

pub(super) fn extract_login_token(request_target: &str) -> Option<String> {
    let url = Url::parse(&format!("http://localhost{request_target}")).ok()?;
    if url.path() != "/sso" {
        return None;
    }

    url.query_pairs().find_map(|(key, value)| {
        (key == "loginToken" && !value.is_empty()).then(|| value.into_owned())
    })
}

pub(super) fn extract_callback_error(query: &str) -> Option<String> {
    Url::parse(&format!("http://localhost/sso?{query}"))
        .ok()?
        .query_pairs()
        .find(|(key, _)| key == "error")
        .map(|(_, value)| value.into_owned())
}

pub(super) fn extract_callback_query(request_target: &str) -> Option<String> {
    let url = Url::parse(&format!("http://localhost{request_target}")).ok()?;
    if url.path() != "/sso" {
        return None;
    }

    let query = url.query()?.trim();
    (!query.is_empty()).then(|| query.to_owned())
}

pub(super) fn write_html_response(stream: &mut TcpStream, status: &str, body: &str) -> Result<(), std::io::Error> {
    let response = format!(
        "HTTP/1.1 {status}\r\nContent-Type: text/html; charset=utf-8\r\nContent-Length: {}\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n{}",
        body.as_bytes().len(),
        body
    );
    stream.write_all(response.as_bytes())?;
    stream.flush()
}

pub(super) fn set_sso_callback_result(
    entry: &Arc<SsoListenerEntry>,
    success: bool,
    login_token: String,
    callback_query: String,
) {
    let mut result = entry
        .result
        .lock()
        .expect("poisoned SSO listener result mutex");
    if result.is_none() {
        *result = Some(SsoCallbackResult {
            success,
            login_token,
            callback_query,
        });
    }
}

pub(super) fn join_sso_listener(entry: &Arc<SsoListenerEntry>) {
    let handle = entry
        .join_handle
        .lock()
        .expect("poisoned SSO listener handle mutex")
        .take();
    if let Some(handle) = handle {
        let _ = handle.join();
    }
}

pub(super) fn html_escape(s: &str) -> String {
    s.replace('&', "&amp;")
        .replace('<', "&lt;")
        .replace('>', "&gt;")
        .replace('"', "&quot;")
        .replace('\'', "&#x27;")
}
