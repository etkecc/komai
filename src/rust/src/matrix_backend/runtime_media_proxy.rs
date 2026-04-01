// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::HashMap;
use std::convert::Infallible;
use std::pin::Pin;
use std::sync::{
    Arc, RwLock,
    atomic::{AtomicBool, Ordering},
};
use std::task::{Context, Poll};
use std::thread::JoinHandle;

use futures_util::StreamExt;
use hyper::{Request, Response, StatusCode, header};
use hyper::body::{Bytes, Frame, Incoming, SizeHint};
use http_body_util::{BodyExt, StreamBody};
use matrix_sdk::ruma::events::room::MediaSource;
use rand::RngExt;

use super::*;

// ---------------------------------------------------------------------------
// Public types
// ---------------------------------------------------------------------------

pub(super) struct MediaProxyInstance {
    pub port: u16,
    registry: Arc<RwLock<HashMap<String, MediaRegistration>>>,
    stop_requested: Arc<AtomicBool>,
    thread: JoinHandle<()>,
}

#[derive(Clone)]
struct MediaRegistration {
    server_name: String,
    media_id: String,
}

struct ProxyState {
    client: Client,
    registry: Arc<RwLock<HashMap<String, MediaRegistration>>>,
}

type BoxBody = http_body_util::combinators::BoxBody<Bytes, std::io::Error>;

/// Wraps a `StreamBody` with an optional known content length.
///
/// hyper uses `Body::size_hint()` to decide between identity and chunked
/// transfer encoding.  A plain `StreamBody` always returns an unknown size,
/// so hyper defaults to chunked — which strips the `Content-Length` header.
/// FFmpeg (via QMediaPlayer) needs `Content-Length` to seek for the moov atom
/// in MP4 files.  This wrapper feeds the upstream `Content-Length` back into
/// `size_hint()`, making hyper use identity encoding and preserving the header.
struct SizedStreamBody<B> {
    inner: B,
    content_length: Option<u64>,
}

impl<B> hyper::body::Body for SizedStreamBody<B>
where
    B: hyper::body::Body<Data = Bytes, Error = std::io::Error> + Unpin,
{
    type Data = Bytes;
    type Error = std::io::Error;

    fn poll_frame(
        self: Pin<&mut Self>,
        cx: &mut Context<'_>,
    ) -> Poll<Option<Result<Frame<Bytes>, Self::Error>>> {
        let this = self.get_mut();
        Pin::new(&mut this.inner).poll_frame(cx)
    }

    fn size_hint(&self) -> SizeHint {
        match self.content_length {
            Some(len) => SizeHint::with_exact(len),
            None => self.inner.size_hint(),
        }
    }
}

// ---------------------------------------------------------------------------
// Public API (called from FFI bridge)
// ---------------------------------------------------------------------------

pub fn start_media_proxy(handle_id: u64) -> Result<u16, String> {
    let client = {
        let mut handles = backend_handles()
            .lock()
            .expect("poisoned matrix backend handle registry mutex");
        let Some(handle) = handles.get_mut(&handle_id) else {
            return Err(format!(
                "matrix-sdk backend runtime handle {handle_id} is not active"
            ));
        };

        if let Some(proxy) = handle.media_proxy.as_ref() {
            if !proxy.thread.is_finished() {
                tracing::debug!(handle_id, port = proxy.port, "Media proxy is already running");
                return Ok(proxy.port);
            }
        }

        handle.client.clone()
    };

    let listener = crate::matrix_backend::ffi::runtime()
        .block_on(tokio::net::TcpListener::bind("127.0.0.1:0"))
        .map_err(|e| format!("failed to bind media proxy listener: {e}"))?;

    let port = listener
        .local_addr()
        .map_err(|e| format!("failed to read media proxy local address: {e}"))?
        .port();

    let registry: Arc<RwLock<HashMap<String, MediaRegistration>>> =
        Arc::new(RwLock::new(HashMap::new()));

    let stop_requested = Arc::new(AtomicBool::new(false));
    let stop_for_thread = Arc::clone(&stop_requested);
    let registry_for_thread = Arc::clone(&registry);

    let thread = std::thread::spawn(move || {
        crate::matrix_backend::ffi::runtime().block_on(run_proxy_loop(
            handle_id,
            listener,
            client,
            registry_for_thread,
            stop_for_thread,
        ));
    });

    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .entry(handle_id)
        .and_modify(|handle| {
            handle.media_proxy = Some(MediaProxyInstance {
                port,
                registry,
                stop_requested,
                thread,
            });
        });

    tracing::info!(handle_id, port, "Started media proxy");
    Ok(port)
}

pub fn is_timeline_media_encrypted(handle_id: u64, item_id: &str) -> bool {
    let item_id = item_id.trim();
    if item_id.is_empty() {
        return false;
    }

    backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get(&handle_id)
        .and_then(|handle| {
            handle
                .room_timeline_media_lookup
                .lock()
                .expect("poisoned matrix room timeline media lookup mutex")
                .get(item_id)
                .map(|req| matches!(req.source, MediaSource::Encrypted(_)))
        })
        .unwrap_or(false)
}

pub fn register_timeline_media_proxy_url(
    handle_id: u64,
    item_id: &str,
    file_extension: &str,
) -> Result<String, String> {
    let item_id = item_id.trim();
    if item_id.is_empty() {
        return Err("cannot register a media proxy URL without an item id".to_owned());
    }

    let handles = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex");

    let handle = handles.get(&handle_id).ok_or_else(|| {
        format!("matrix-sdk backend runtime handle {handle_id} is not active")
    })?;

    let proxy = handle.media_proxy.as_ref().ok_or_else(|| {
        format!("media proxy is not running for handle {handle_id}")
    })?;

    let media_request = handle
        .room_timeline_media_lookup
        .lock()
        .expect("poisoned matrix room timeline media lookup mutex")
        .get(item_id)
        .cloned()
        .ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no active timeline media for item '{item_id}'"
            )
        })?;

    let mxc_uri = match &media_request.source {
        MediaSource::Plain(uri) => uri.to_string(),
        MediaSource::Encrypted(_) => {
            return Err("encrypted media cannot be streamed via the proxy".to_owned());
        }
    };

    let (server_name, media_id) = parse_mxc_uri(&mxc_uri)?;

    let token = generate_token();
    let ext = file_extension.trim().trim_start_matches('.');

    let url = if ext.is_empty() {
        format!("http://localhost:{}/m/{token}", proxy.port)
    } else {
        format!("http://localhost:{}/m/{token}.{ext}", proxy.port)
    };

    proxy
        .registry
        .write()
        .expect("poisoned media proxy registry lock")
        .insert(
            token,
            MediaRegistration {
                server_name: server_name.to_owned(),
                media_id: media_id.to_owned(),
            },
        );

    Ok(url)
}

pub fn stop_media_proxy(handle_id: u64) {
    let proxy = backend_handles()
        .lock()
        .expect("poisoned matrix backend handle registry mutex")
        .get_mut(&handle_id)
        .and_then(|handle| handle.media_proxy.take());

    if let Some(proxy) = proxy {
        stop_proxy_instance(handle_id, proxy);
    }
}

pub(super) fn stop_proxy_instance(handle_id: u64, proxy: MediaProxyInstance) {
    tracing::info!(handle_id, port = proxy.port, "Stopping media proxy");
    proxy.stop_requested.store(true, Ordering::Relaxed);
    let _ = proxy.thread.join();
}

// ---------------------------------------------------------------------------
// Proxy event loop
// ---------------------------------------------------------------------------

async fn run_proxy_loop(
    handle_id: u64,
    listener: tokio::net::TcpListener,
    client: Client,
    registry: Arc<RwLock<HashMap<String, MediaRegistration>>>,
    stop_requested: Arc<AtomicBool>,
) {
    let state = Arc::new(ProxyState { client, registry });

    loop {
        let accept_result = tokio::select! {
            result = listener.accept() => result,
            () = poll_stop(&stop_requested) => break,
        };

        let (stream, _) = match accept_result {
            Ok(conn) => conn,
            Err(e) => {
                tracing::warn!(handle_id, "Media proxy accept error: {e}");
                continue;
            }
        };

        let state = Arc::clone(&state);
        tokio::spawn(async move {
            let io = hyper_util::rt::TokioIo::new(stream);
            let service = hyper::service::service_fn(move |req| {
                let state = Arc::clone(&state);
                handle_request(req, state)
            });
            if let Err(e) = hyper::server::conn::http1::Builder::new()
                .serve_connection(io, service)
                .await
            {
                // Connection reset / incomplete message is normal when clients disconnect.
                tracing::debug!("Media proxy connection closed: {e}");
            }
        });
    }

    tracing::info!(handle_id, "Media proxy loop stopped");
}

async fn poll_stop(flag: &AtomicBool) {
    loop {
        if flag.load(Ordering::Relaxed) {
            return;
        }
        tokio::time::sleep(Duration::from_millis(100)).await;
    }
}

// ---------------------------------------------------------------------------
// Request handling
// ---------------------------------------------------------------------------

async fn handle_request(
    req: Request<Incoming>,
    state: Arc<ProxyState>,
) -> Result<Response<BoxBody>, Infallible> {
    let path = req.uri().path();

    let Some(token) = parse_media_token(path) else {
        return Ok(text_response(StatusCode::NOT_FOUND, "not found"));
    };

    let registration = state
        .registry
        .read()
        .expect("poisoned media proxy registry lock")
        .get(token)
        .cloned();

    let Some(reg) = registration else {
        return Ok(text_response(StatusCode::NOT_FOUND, "unknown media token"));
    };

    let Some(access_token) = state.client.access_token() else {
        return Ok(text_response(
            StatusCode::SERVICE_UNAVAILABLE,
            "no access token available",
        ));
    };

    let upstream_url = format!(
        "{}/_matrix/client/v1/media/download/{}/{}",
        state.client.homeserver().as_str().trim_end_matches('/'),
        reg.server_name,
        reg.media_id,
    );

    let mut upstream_req = state
        .client
        .http_client()
        .get(&upstream_url)
        .bearer_auth(&access_token);

    // Forward Range header for seeking support.
    if let Some(range_value) = req.headers().get(header::RANGE) {
        upstream_req = upstream_req.header(reqwest::header::RANGE, range_value.as_bytes());
    }

    let upstream_resp = match upstream_req.send().await {
        Ok(r) => r,
        Err(e) => {
            tracing::warn!("Media proxy upstream request failed: {e}");
            return Ok(text_response(StatusCode::BAD_GATEWAY, "upstream request failed"));
        }
    };

    let status = upstream_resp.status();
    let content_length = upstream_resp.content_length();
    let mut builder = Response::builder().status(status.as_u16());

    // Close the connection after each response.  FFmpeg/QMediaPlayer may seek
    // by issuing a new Range request on the same keep-alive connection before
    // fully consuming the previous response body.  This causes hyper's leftover
    // body bytes to be misinterpreted as the next response, corrupting the stream.
    builder = builder.header(header::CONNECTION, "close");

    // Forward relevant headers.
    for name in [
        reqwest::header::CONTENT_TYPE,
        reqwest::header::CONTENT_LENGTH,
        reqwest::header::CONTENT_RANGE,
        reqwest::header::ACCEPT_RANGES,
    ] {
        if let Some(val) = upstream_resp.headers().get(&name) {
            if let Ok(val) = val.to_str() {
                builder = builder.header(name.as_str(), val);
            }
        }
    }

    // Serve the body back to the client.
    //
    // When the upstream provides Content-Length, stream chunk by chunk with a
    // SizedStreamBody so hyper uses identity encoding (preserving Content-Length).
    //
    // When the upstream uses chunked encoding (no Content-Length), buffer the
    // full response and serve with an explicit Content-Length.  FFmpeg/QMediaPlayer
    // needs Content-Length to seek for the moov atom in MP4 files; without it,
    // video playback silently fails.  The memory cost matches the existing
    // buffer-download fallback path.
    if let Some(len) = content_length {
        let body_stream = upstream_resp.bytes_stream().map(|result| {
            result
                .map(|bytes| Frame::data(bytes))
                .map_err(|e| std::io::Error::other(e))
        });
        let stream_body = SizedStreamBody {
            inner: StreamBody::new(body_stream),
            content_length: Some(len),
        };
        Ok(builder.body(BodyExt::boxed(stream_body)).expect("valid response"))
    } else {
        let full_body = upstream_resp.bytes().await.map_err(|e| {
            tracing::warn!("Media proxy failed to buffer upstream body: {e}");
            e
        }).unwrap_or_default();
        builder = builder.header(header::CONTENT_LENGTH, full_body.len());
        let body = http_body_util::Full::new(full_body)
            .map_err(|never| match never {});
        Ok(builder.body(body.boxed()).expect("valid response"))
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Extracts the token from a path like `/m/{token}` or `/m/{token}.ext`.
fn parse_media_token(path: &str) -> Option<&str> {
    let rest = path.strip_prefix("/m/")?;
    if rest.is_empty() {
        return None;
    }
    // Strip optional file extension (cosmetic — ignored by routing).
    Some(rest.split('.').next().unwrap_or(rest))
}

/// Parses `mxc://server_name/media_id` into `(server_name, media_id)`.
fn parse_mxc_uri(uri: &str) -> Result<(&str, &str), String> {
    let rest = uri
        .strip_prefix("mxc://")
        .ok_or_else(|| format!("not a valid mxc:// URI: '{uri}'"))?;
    rest.split_once('/')
        .filter(|(s, m)| !s.is_empty() && !m.is_empty())
        .ok_or_else(|| format!("invalid mxc URI format: '{uri}'"))
}

fn generate_token() -> String {
    const CHARSET: &[u8] = b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    let mut rng = rand::rng();
    (0..24)
        .map(|_| CHARSET[rng.random_range(0..CHARSET.len())] as char)
        .collect()
}

fn text_response(status: StatusCode, body: &str) -> Response<BoxBody> {
    let full = http_body_util::Full::new(Bytes::from(body.to_owned()))
        .map_err(|never| match never {});
    Response::builder()
        .status(status)
        .header(header::CONTENT_TYPE, "text/plain")
        .body(full.boxed())
        .expect("valid response")
}
