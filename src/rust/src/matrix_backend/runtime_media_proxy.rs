// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::HashMap;
use std::convert::Infallible;
use std::pin::Pin;
use std::sync::{
    Arc, OnceLock, RwLock,
    atomic::{AtomicBool, Ordering},
};
use std::task::{Context, Poll};
use std::thread::JoinHandle;

use futures_util::StreamExt;
use http_body_util::{BodyExt, StreamBody};
use hyper::body::{Bytes, Frame, Incoming, SizeHint};
use hyper::{Request, Response, StatusCode, header};
use matrix_sdk::ruma::events::room::MediaSource;
use rand::RngExt;

use super::*;

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

/// Maximum response body size that the proxy will cache in memory per token.
///
/// ## Why cache at all?
///
/// FFmpeg (used by QMediaPlayer) cannot seek within an HTTP response body —
/// HTTP is a forward-only stream.  When FFmpeg encounters an MP4 file with
/// the `moov` atom at the end (the common layout for phone-recorded video),
/// it needs to seek backwards to read the file index.  The only way to seek
/// over HTTP is via Range requests (`Range: bytes=START-END`).
///
/// Many Matrix homeservers (especially those behind reverse proxies) do not
/// support HTTP Range requests — they return the full file with chunked
/// transfer encoding regardless.  Without local Range support in the proxy,
/// FFmpeg's seek fails, playback errors out, and the MxcMediaProxy fallback
/// downloads the entire file a second time into a local QBuffer.
///
/// By caching the response body after the first GET, the proxy can serve
/// subsequent Range requests locally — slicing the cached bytes and returning
/// 206 Partial Content.  This gives FFmpeg the seek capability it needs and
/// avoids the double-download fallback.
///
/// ## Size limit
///
/// Bodies larger than this threshold are NOT cached.  For very large files
/// (e.g., a 200 MB video), holding the full body in memory would be wasteful.
/// Those files fall back to the MxcMediaProxy buffer-download path, which
/// writes to disk and plays from there.
///
/// The cache is per-token and the entire registry (including cached bodies)
/// is destroyed on logout / backend stop.
const BODY_CACHE_MAX_BYTES: usize = 150 * 1024 * 1024; // 150 MB

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

pub(super) struct MediaProxyInstance {
    pub port: u16,
    registry: Arc<RwLock<HashMap<String, MediaRegistration>>>,
    stop_requested: Arc<AtomicBool>,
    thread: JoinHandle<()>,
}

/// Per-token registration mapping a proxy token to upstream media coordinates.
///
/// The `body_cache` field holds the first GET response body so that subsequent
/// requests (including Range seeks) can be served locally.  See the
/// [`BODY_CACHE_MAX_BYTES`] doc comment for the full rationale.
#[derive(Clone)]
struct MediaRegistration {
    server_name: String,
    media_id: String,
    body_cache: Arc<OnceLock<CachedBody>>,
}

/// A cached upstream response body with its Content-Type.
#[derive(Clone)]
struct CachedBody {
    data: Bytes,
    content_type: String,
}

struct ProxyState {
    client: Client,
    registry: Arc<RwLock<HashMap<String, MediaRegistration>>>,
}

type BoxBody = http_body_util::combinators::BoxBody<Bytes, std::io::Error>;

/// Wraps a `StreamBody` with a known content length so hyper uses identity
/// encoding instead of chunked.
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
                body_cache: Arc::new(OnceLock::new()),
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

    let range_header = req.headers().get(header::RANGE).cloned();

    // ── Serve from body cache (fastest path) ────────────────────────────
    //
    // If a previous request already cached the response body, serve
    // directly from memory.  For Range requests, parse the byte range and
    // return 206 Partial Content — this is the key feature that enables
    // FFmpeg to seek to the moov atom in MP4 files without upstream Range
    // support.
    if let Some(cached) = reg.body_cache.get() {
        return Ok(serve_from_cache(cached, range_header.as_ref()));
    }

    // ── Cache miss: try upstream with Range, fall back to buffer+cache ──
    //
    // FFmpeg always opens URLs with `Range: bytes=0-` as the very first
    // request (never a plain GET).
    //
    // Strategy:
    // 1. Forward the client's request (including Range header) to upstream.
    // 2. If upstream returns 206 (supports Range): stream the partial content
    //    directly — no buffering, no caching.  FFmpeg will make further Range
    //    requests for seeking and each will be forwarded to upstream.  This is
    //    the ideal fast path for servers with Range support.
    // 3. If upstream returns 200 (ignores Range): buffer the full body, cache
    //    it, and serve locally.  Subsequent Range requests (for moov-atom
    //    seeking etc.) will be served from the cache.

    let mut upstream_req = state
        .client
        .http_client()
        .get(&upstream_url)
        .bearer_auth(&access_token);

    // Forward the client's Range header to probe upstream support.
    if let Some(range_val) = range_header.as_ref() {
        upstream_req = upstream_req.header(reqwest::header::RANGE, range_val.as_bytes());
    }

    let upstream_resp = match upstream_req.send().await {
        Ok(r) => r,
        Err(e) => {
            tracing::warn!("Media proxy upstream request failed: {e}");
            return Ok(text_response(
                StatusCode::BAD_GATEWAY,
                "upstream request failed",
            ));
        }
    };

    let upstream_status = upstream_resp.status();

    if !upstream_status.is_success() && upstream_status != reqwest::StatusCode::PARTIAL_CONTENT {
        tracing::warn!("Media proxy upstream returned {upstream_status}");
        return Ok(text_response(
            StatusCode::from_u16(upstream_status.as_u16()).unwrap_or(StatusCode::BAD_GATEWAY),
            "upstream error",
        ));
    }

    // ── Fast path: upstream supports Range (206) → stream directly ──────
    if upstream_status == reqwest::StatusCode::PARTIAL_CONTENT {
        let mut builder = Response::builder()
            .status(StatusCode::PARTIAL_CONTENT)
            .header(header::CONNECTION, "close");

        // Forward Content-Type, Content-Length, Content-Range, Accept-Ranges.
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

        // Stream the partial content — no need to buffer since upstream
        // handles Range natively.  FFmpeg will make further Range requests
        // for seeking, each forwarded to upstream individually.
        let content_length = upstream_resp.content_length();
        let body_stream = upstream_resp.bytes_stream().map(|result| {
            result
                .map(|bytes| Frame::data(bytes))
                .map_err(|e| std::io::Error::other(e))
        });
        let stream_body = SizedStreamBody {
            inner: StreamBody::new(body_stream),
            content_length,
        };

        return Ok(builder.body(BodyExt::boxed(stream_body)).expect("valid response"));
    }

    // ── Slow path: upstream returned 200 (no Range) → buffer and cache ──
    let content_type = upstream_resp
        .headers()
        .get(reqwest::header::CONTENT_TYPE)
        .and_then(|v| v.to_str().ok())
        .unwrap_or("application/octet-stream")
        .to_owned();

    let full_body = upstream_resp.bytes().await.unwrap_or_default();

    // Cache the body for this and all future requests.
    if full_body.len() <= BODY_CACHE_MAX_BYTES {
        let _ = reg.body_cache.set(CachedBody {
            data: full_body.clone(),
            content_type: content_type.clone(),
        });
    }

    // Serve from cache if populated (handles Range slicing).
    if let Some(cached) = reg.body_cache.get() {
        return Ok(serve_from_cache(cached, range_header.as_ref()));
    }

    // Body too large to cache — serve the full buffered response directly.
    // Range seeking won't work; the MxcMediaProxy fallback will handle it.
    let builder = Response::builder()
        .status(StatusCode::OK)
        .header(header::CONNECTION, "close")
        .header(header::CONTENT_TYPE, &content_type)
        .header(header::CONTENT_LENGTH, full_body.len());

    let body = http_body_util::Full::new(full_body).map_err(|never| match never {});
    Ok(builder.body(body.boxed()).expect("valid response"))
}

// ---------------------------------------------------------------------------
// Cache-based serving
// ---------------------------------------------------------------------------

/// Serve a full or partial response from the in-memory body cache.
///
/// For plain GET requests: returns the full cached body with `Accept-Ranges: bytes`.
/// For Range requests: parses the byte range and returns 206 with the requested slice.
fn serve_from_cache(
    cached: &CachedBody,
    range_header: Option<&hyper::header::HeaderValue>,
) -> Response<BoxBody> {
    let total_len = cached.data.len();

    // Parse and serve Range request from cache.
    if let Some(range_val) = range_header {
        if let Some(range_str) = range_val.to_str().ok() {
            if let Some((start, end)) = parse_byte_range(range_str, total_len) {
                let slice = cached.data.slice(start..=end);
                let content_range =
                    format!("bytes {start}-{end}/{total_len}");

                let body = http_body_util::Full::new(slice).map_err(|never| match never {});
                return Response::builder()
                    .status(StatusCode::PARTIAL_CONTENT)
                    .header(header::CONNECTION, "close")
                    .header(header::CONTENT_TYPE, &cached.content_type)
                    .header(header::CONTENT_LENGTH, end - start + 1)
                    .header(header::CONTENT_RANGE, content_range)
                    .header(header::ACCEPT_RANGES, "bytes")
                    .body(body.boxed())
                    .expect("valid response");
            }
        }

        // Unparseable Range header — return 416.
        let body = http_body_util::Full::new(Bytes::from("invalid range"))
            .map_err(|never| match never {});
        return Response::builder()
            .status(StatusCode::RANGE_NOT_SATISFIABLE)
            .header(header::CONNECTION, "close")
            .header(
                header::CONTENT_RANGE,
                format!("bytes */{total_len}"),
            )
            .body(body.boxed())
            .expect("valid response");
    }

    // Full GET from cache.
    let body =
        http_body_util::Full::new(cached.data.clone()).map_err(|never| match never {});
    Response::builder()
        .status(StatusCode::OK)
        .header(header::CONNECTION, "close")
        .header(header::CONTENT_TYPE, &cached.content_type)
        .header(header::CONTENT_LENGTH, total_len)
        .header(header::ACCEPT_RANGES, "bytes")
        .body(body.boxed())
        .expect("valid response")
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

/// Parses an HTTP `Range` header value like `bytes=START-END`.
///
/// Supports:
/// - `bytes=START-END` (inclusive range)
/// - `bytes=START-` (from START to end of file)
/// - `bytes=-SUFFIX` (last SUFFIX bytes)
///
/// Returns `(start, end)` as inclusive byte offsets, or `None` if unparseable.
fn parse_byte_range(header: &str, total_len: usize) -> Option<(usize, usize)> {
    if total_len == 0 {
        return None;
    }
    let range = header.strip_prefix("bytes=")?;
    if let Some(suffix_str) = range.strip_prefix('-') {
        let suffix: usize = suffix_str.parse().ok()?;
        if suffix == 0 {
            return None;
        }
        let start = total_len.saturating_sub(suffix);
        Some((start, total_len - 1))
    } else {
        let (start_str, end_str) = range.split_once('-')?;
        let start: usize = start_str.parse().ok()?;
        if start >= total_len {
            return None;
        }
        let end = if end_str.is_empty() {
            total_len - 1
        } else {
            let end: usize = end_str.parse().ok()?;
            end.min(total_len - 1)
        };
        if start > end {
            return None;
        }
        Some((start, end))
    }
}

fn generate_token() -> String {
    const CHARSET: &[u8] = b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    let mut rng = rand::rng();
    (0..24)
        .map(|_| CHARSET[rng.random_range(0..CHARSET.len())] as char)
        .collect()
}

fn text_response(status: StatusCode, body: &str) -> Response<BoxBody> {
    let full =
        http_body_util::Full::new(Bytes::from(body.to_owned())).map_err(|never| match never {});
    Response::builder()
        .status(status)
        .header(header::CONTENT_TYPE, "text/plain")
        .body(full.boxed())
        .expect("valid response")
}
