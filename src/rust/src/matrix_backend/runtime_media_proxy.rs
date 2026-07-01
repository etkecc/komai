// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::HashMap;
use std::convert::Infallible;
use std::pin::Pin;
use std::sync::{
    Arc, Mutex, OnceLock, RwLock,
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

/// Maximum body size the proxy will hold in memory for the stream-through path.
///
/// ## Why hold the body in memory at all?
///
/// FFmpeg (used by QMediaPlayer) cannot seek within an HTTP response body —
/// HTTP is a forward-only stream.  When FFmpeg encounters an MP4 file with
/// the `moov` atom at the end (the common layout for phone-recorded video),
/// it needs to seek backwards to read the file index.  The only way to seek
/// over HTTP is via Range requests (`Range: bytes=START-END`).
///
/// Many Matrix homeservers (especially those behind reverse proxies) do not
/// support HTTP Range requests — they return the full file with a `200` and
/// chunked transfer encoding regardless.  For those, the proxy downloads the
/// body once into a shared growing buffer and serves every request (including
/// FFmpeg's moov seek) from that buffer, returning 206 Partial Content so
/// FFmpeg believes the stream is seekable.  See [`StreamProgress`].
///
/// ## Size limit
///
/// Bodies larger than this threshold are NOT held in memory.  For very large
/// files (e.g., a 200 MB video), buffering the whole body would be wasteful.
/// Those fall back to the MxcMediaProxy client download path, which writes to
/// disk and plays from there.
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
/// The `stream` field holds the shared, growing download buffer used by the
/// no-Range stream-through path.  See [`StreamProgress`].
#[derive(Clone)]
struct MediaRegistration {
    server_name: String,
    media_id: String,
    stream: Arc<Mutex<StreamProgress>>,
}

/// Shared, growing state for a single no-Range download.
///
/// When an upstream server ignores Range requests (returns `200` instead of
/// `206`), a single background task downloads the body once, appending to
/// `buffer` as bytes arrive.  Every client request for this token — the first
/// `bytes=0-` and any subsequent seek — is served as a "follower" that streams
/// from `buffer`, waiting when it catches up to the download frontier.
///
/// This keeps FFmpeg continuously fed (so it never hits its ~20s HTTP
/// read-timeout) and lets seek requests be served from the growing buffer.
/// State is per-token and each open registers a fresh token, so every playback
/// re-streams — there is no cross-playback cache to short-circuit testing.
#[derive(Default)]
struct StreamProgress {
    /// A download task is currently filling `buffer`.
    downloading: bool,
    /// The download finished; `buffer` holds the complete body.
    complete: bool,
    /// The download aborted before completing.
    failed: bool,
    /// Total body length, known up-front from the upstream `Content-Length`.
    total: Option<usize>,
    /// Content-Type of the body, echoed back to followers.
    content_type: String,
    /// Bytes downloaded so far, in file order from offset 0.
    buffer: Vec<u8>,
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
                stream: Arc::new(Mutex::new(StreamProgress::default())),
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

    // Trace the request sequence FFmpeg issues (one line per request) so the
    // no-Range streaming behaviour is observable in the logs — kept always-on.
    tracing::info!(
        token = %short_token(token),
        range = ?range_str(range_header.as_ref()),
        "media proxy: request in"
    );

    // ── Follow an in-progress (or finished) no-Range download ───────────
    //
    // A previous request already probed upstream, found it ignores Range,
    // and started a single background download into a shared buffer.  Serve
    // this request (typically FFmpeg seeking back for the moov atom) from
    // that buffer instead of re-fetching the whole body from upstream.
    {
        let prog = reg.stream.lock().expect("poisoned media proxy stream lock");
        if prog.downloading || prog.complete {
            let content_type = prog.content_type.clone();
            let total = prog.total;
            let buffered = prog.buffer.len();
            let complete = prog.complete;
            drop(prog);
            tracing::info!(
                token = %short_token(token),
                range = ?range_str(range_header.as_ref()),
                buffered,
                total,
                complete,
                "media proxy: following existing download"
            );
            return Ok(serve_as_follower(
                Arc::clone(&reg.stream),
                &content_type,
                total,
                range_header.as_ref(),
            ));
        }
    }

    // ── First request for this token: probe upstream Range support ──────
    //
    // FFmpeg always opens URLs with `Range: bytes=0-` as the very first
    // request (never a plain GET).
    //
    // Strategy:
    // 1. Forward the client's request (including Range header) to upstream.
    // 2. If upstream returns 206 (supports Range): stream the partial content
    //    directly — no buffering.  FFmpeg will make further Range requests for
    //    seeking and each is forwarded to upstream.  Ideal fast path for
    //    servers with Range support.
    // 3. If upstream returns 200 (ignores Range): start a single stream-through
    //    download into a shared buffer and serve this and every later request
    //    (including moov-atom seeks) from that growing buffer.

    let mut upstream_req = state
        .client
        .http_client()
        .get(&upstream_url)
        // matrix-sdk's shared HTTP client has a ~30s request timeout, which
        // aborts a slow full-body download mid-stream (surfacing as an "error
        // decoding response body"). Media downloads can legitimately take much
        // longer, so override it with a generous per-request deadline. The
        // client-side streaming watchdog handles genuinely stuck connections.
        .timeout(Duration::from_secs(3600))
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

    // ── Slow path: upstream returned 200 (ignores Range) ────────────────
    //
    // Only audio is streamed forward here.  Video is deliberately excluded:
    // the media overlay loops playback (`loops: Infinite`), and looping seeks
    // back to offset 0 — impossible on a non-seekable forward stream (FFmpeg
    // reports "Function not implemented"), so a streamed video glitches at the
    // loop boundary.  High-bitrate video also stutters over a constrained link,
    // and moov-at-end video can't be decoded forward at all.  So for anything
    // that isn't audio we fail fast, and the client downloads the file and
    // plays it from a seekable local buffer instead.
    //
    // Audio has none of those problems (no loop-seek, low bitrate), so it is
    // streamed: the body is downloaded once into a shared growing buffer while
    // its bytes are fed to the client, avoiding FFmpeg's ~20s read-timeout.
    let content_type = upstream_resp
        .headers()
        .get(reqwest::header::CONTENT_TYPE)
        .and_then(|v| v.to_str().ok())
        .unwrap_or("application/octet-stream")
        .to_owned();
    let total = upstream_resp.content_length().map(|v| v as usize);

    if !content_type.starts_with("audio/") {
        // Not forward-streamable in practice → fail fast to the client's
        // full-download path, which plays from a seekable buffer and loops.
        drop(upstream_resp);
        tracing::info!(
            token = %short_token(token),
            content_type = %content_type,
            "media proxy: no-Range 200, failing fast to client download (non-audio)"
        );
        return Ok(text_response(
            StatusCode::BAD_GATEWAY,
            "media requires full download",
        ));
    }

    // Only the first request drives the download; concurrent requests follow
    // the shared buffer it fills.
    let claimed = {
        let mut prog = reg.stream.lock().expect("poisoned media proxy stream lock");
        if prog.downloading || prog.complete {
            false
        } else {
            prog.downloading = true;
            prog.total = total;
            prog.content_type = content_type.clone();
            prog.buffer = Vec::with_capacity(total.unwrap_or(0));
            true
        }
    };

    if claimed {
        tracing::info!(
            token = %short_token(token),
            ?total,
            content_type = %content_type,
            "media proxy: no-Range 200, streaming audio"
        );
        tokio::spawn(run_stream_download(
            Arc::clone(&reg.stream),
            Box::pin(upstream_resp.bytes_stream()),
            token.to_owned(),
        ));
    } else {
        drop(upstream_resp);
        tracing::info!(
            token = %short_token(token),
            "media proxy: no-Range 200, following concurrent audio stream"
        );
    }

    Ok(serve_as_follower(
        Arc::clone(&reg.stream),
        &content_type,
        total,
        range_header.as_ref(),
    ))
}

// ---------------------------------------------------------------------------
// Stream-through serving (no-Range upstream)
// ---------------------------------------------------------------------------

/// Drain the remaining upstream body into the shared buffer as bytes arrive,
/// so followers can stream from it.  The buffer is pre-seeded with the bytes
/// already read while sniffing the container, so `received` starts from there.
/// Runs decoupled from any client connection, so an FFmpeg seek that closes its
/// current connection does not abort the download.
async fn run_stream_download(
    stream: Arc<Mutex<StreamProgress>>,
    mut body: Pin<Box<dyn futures_util::Stream<Item = reqwest::Result<Bytes>> + Send>>,
    token: String,
) {
    let throttle_kbps = media_throttle_kbps();
    let mut received = stream
        .lock()
        .expect("poisoned media proxy stream lock")
        .buffer
        .len();
    let mut ok = true;
    let mut warned_oversize = false;

    while let Some(item) = body.next().await {
        match item {
            Ok(chunk) => {
                received += chunk.len();
                {
                    let mut prog =
                        stream.lock().expect("poisoned media proxy stream lock");
                    prog.buffer.extend_from_slice(&chunk);
                }
                // The whole body is held in memory so followers can seek back.
                // Flag the (rare) case where it grows past the intended budget.
                if !warned_oversize && received > BODY_CACHE_MAX_BYTES {
                    warned_oversize = true;
                    tracing::warn!(
                        token = %short_token(&token),
                        received,
                        limit = BODY_CACHE_MAX_BYTES,
                        "media proxy: stream-through body exceeds in-memory budget"
                    );
                }
                // Optional: pace the download to simulate a slow homeserver
                // (KOMAI_DEBUG_MEDIA_THROTTLE_KBPS), for reproducing the
                // no-Range slow path without a real slow server.
                if let Some(kbps) = throttle_kbps {
                    let ms = (chunk.len() as u64 * 1000) / (kbps * 1024).max(1);
                    if ms > 0 {
                        tokio::time::sleep(Duration::from_millis(ms)).await;
                    }
                }
            }
            Err(e) => {
                tracing::warn!(
                    token = %short_token(&token),
                    received,
                    "media proxy: stream-through download error: {e}"
                );
                ok = false;
                break;
            }
        }
    }

    let mut prog = stream.lock().expect("poisoned media proxy stream lock");
    prog.downloading = false;
    if ok {
        prog.complete = true;
        // If upstream never sent a Content-Length, we now know the real total.
        if prog.total.is_none() {
            prog.total = Some(received);
        }
        tracing::info!(
            token = %short_token(&token),
            received,
            "media proxy: stream-through download complete"
        );
    } else {
        prog.failed = true;
    }
}

/// Serve a request by following the shared download buffer: send response
/// headers immediately, then stream bytes as they become available, waiting
/// when caught up to the download frontier.
///
/// With a known `total` we can answer Range requests as seekable `206`s.
/// Without one (Synapse's chunked 200) we stream forward as a non-seekable
/// `200` from offset 0.
fn serve_as_follower(
    stream: Arc<Mutex<StreamProgress>>,
    content_type: &str,
    total: Option<usize>,
    range_header: Option<&hyper::header::HeaderValue>,
) -> Response<BoxBody> {
    // ── Unknown length: non-seekable forward stream (plain 200) ─────────
    let Some(total) = total.filter(|&t| t > 0) else {
        if let Some(raw) = range_str(range_header) {
            if !raw.starts_with("bytes=0-") {
                tracing::warn!(range = %raw, "media proxy: nonzero range on unknown-length body; streaming from 0");
            }
        }
        let body_stream = Box::pin(follower_stream(stream, 0, None));
        let stream_body = SizedStreamBody {
            inner: StreamBody::new(body_stream),
            content_length: None,
        };
        return Response::builder()
            .status(StatusCode::OK)
            .header(header::CONNECTION, "close")
            .header(header::CONTENT_TYPE, content_type)
            .body(BodyExt::boxed(stream_body))
            .expect("valid response");
    };

    // ── Known length: resolve the requested byte range, serve 206/200 ───
    let (start, end, partial) = match range_header.and_then(|v| v.to_str().ok()) {
        Some(range_str) => match parse_byte_range(range_str, total) {
            Some((s, e)) => (s, e, true),
            None => {
                let body = http_body_util::Full::new(Bytes::from("invalid range"))
                    .map_err(|never| match never {});
                return Response::builder()
                    .status(StatusCode::RANGE_NOT_SATISFIABLE)
                    .header(header::CONNECTION, "close")
                    .header(header::CONTENT_RANGE, format!("bytes */{total}"))
                    .body(body.boxed())
                    .expect("valid response");
            }
        },
        None => (0, total.saturating_sub(1), false),
    };

    let content_length = (end + 1 - start) as u64;
    let body_stream = Box::pin(follower_stream(stream, start, Some(end)));
    let stream_body = SizedStreamBody {
        inner: StreamBody::new(body_stream),
        content_length: Some(content_length),
    };

    let mut builder = Response::builder().header(header::CONNECTION, "close");
    if partial {
        builder = builder
            .status(StatusCode::PARTIAL_CONTENT)
            .header(header::CONTENT_RANGE, format!("bytes {start}-{end}/{total}"));
    } else {
        builder = builder.status(StatusCode::OK);
    }
    builder = builder
        .header(header::CONTENT_TYPE, content_type)
        .header(header::CONTENT_LENGTH, content_length)
        .header(header::ACCEPT_RANGES, "bytes");

    builder
        .body(BodyExt::boxed(stream_body))
        .expect("valid response")
}

/// A stream over the shared download buffer starting at `start`.  `end` is the
/// last inclusive offset to serve, or `None` to stream until the download
/// completes.  Yields buffered bytes as they arrive; when caught up to the
/// frontier it polls every 25ms until more data lands, completes, or fails.
fn follower_stream(
    stream: Arc<Mutex<StreamProgress>>,
    start: usize,
    end: Option<usize>,
) -> impl futures_util::Stream<Item = Result<Frame<Bytes>, std::io::Error>> {
    let want_end = end.map(|e| e + 1); // exclusive upper bound, if bounded
    futures_util::stream::unfold((start, stream), move |(cursor, stream)| async move {
        enum Step {
            Chunk(Bytes, usize),
            Wait,
            Fail,
            Done,
        }
        // Sentinel cursor: set after yielding an error frame so the next poll
        // terminates deterministically.
        const TERMINATED: usize = usize::MAX;
        loop {
            if cursor == TERMINATED {
                return None;
            }
            let step = {
                let prog = stream.lock().expect("poisoned media proxy stream lock");
                let upto = match want_end {
                    Some(w) => prog.buffer.len().min(w),
                    None => prog.buffer.len(),
                };
                if cursor < upto {
                    Step::Chunk(Bytes::copy_from_slice(&prog.buffer[cursor..upto]), upto)
                } else if want_end.is_some_and(|w| cursor >= w) || prog.complete {
                    Step::Done
                } else if prog.failed {
                    Step::Fail
                } else {
                    Step::Wait
                }
            };
            match step {
                Step::Chunk(chunk, new_cursor) => {
                    return Some((Ok(Frame::data(chunk)), (new_cursor, stream)));
                }
                Step::Wait => {
                    tokio::time::sleep(Duration::from_millis(25)).await;
                }
                Step::Fail => {
                    return Some((
                        Err(std::io::Error::other("stream-through download failed")),
                        (TERMINATED, stream),
                    ));
                }
                Step::Done => return None,
            }
        }
    })
}

/// Reads `KOMAI_DEBUG_MEDIA_THROTTLE_KBPS` once: an optional download rate cap
/// (in KB/s) used to simulate a slow homeserver.  Unset/invalid → no throttle.
fn media_throttle_kbps() -> Option<u64> {
    static THROTTLE: OnceLock<Option<u64>> = OnceLock::new();
    *THROTTLE.get_or_init(|| {
        std::env::var("KOMAI_DEBUG_MEDIA_THROTTLE_KBPS")
            .ok()
            .and_then(|v| v.trim().parse::<u64>().ok())
            .filter(|&v| v > 0)
    })
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// First 6 characters of a media token, for terse, non-secret log correlation.
fn short_token(token: &str) -> &str {
    &token[..token.len().min(6)]
}

/// Borrow a `Range` header value as `&str` for logging (`None` if absent/invalid).
fn range_str(range_header: Option<&hyper::header::HeaderValue>) -> Option<&str> {
    range_header.and_then(|v| v.to_str().ok())
}

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
