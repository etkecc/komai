// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Realtime / streaming transcription via the OpenAI Realtime transcription
//! protocol (WebSocket).
//!
//! The session is driven from outside the tokio runtime through a
//! [`RealtimeSession`] handle: push PCM16 chunks via [`push_audio`], then
//! [`commit`] when the user releases Space (or [`cancel`] on Esc). Events
//! emitted by the server land in a per-session queue that the C++ side
//! drains on a Qt timer (see `transcription/ffi.rs`).
//!
//! Wire protocol reference:
//! - Connect: `wss://{host}/v1/realtime?intent=transcription`. We
//!   derive the WebSocket URL from the configured HTTP `api_url`,
//!   rewriting the scheme, appending `/realtime` if missing, and
//!   adding `?intent=transcription`. `?intent=transcription` anchors
//!   the session to the transcription category at connect time; an
//!   S2S session would instead carry `?model=<realtime-model-id>` and
//!   the two URL shapes are mutually exclusive.
//! - Auth: `Authorization: Bearer <key>`.
//! - Configure: send a `session.update` event with `session.type:
//!   "transcription"` (required, distinguishes a transcription-only
//!   session from S2S). Format / transcription / turn_detection sit
//!   under `session.audio.input.*`; audio format is the structured
//!   `{type: "audio/pcm", rate: <hz>}` object. Some models (e.g.
//!   `gpt-realtime-whisper`) stream deltas natively and reject
//!   `turn_detection` — they pace output themselves. Other models
//!   (`gpt-4o-mini-transcribe`, `gpt-4o-transcribe`) require server
//!   VAD: without it they buffer all audio until commit. With VAD
//!   enabled, a phrase-length pause triggers an utterance commit and
//!   shortly after the phrase appears; deltas land per utterance
//!   rather than per word.
//! - Stream: `input_audio_buffer.append` with base64 PCM16 chunks. With
//!   VAD enabled, the server transcribes incrementally and emits deltas
//!   while the user is still speaking; VAD also auto-commits each
//!   utterance on detected silence, emitting a `completed` per utterance.
//! - Commit: `input_audio_buffer.commit` on user release. Forces the
//!   server to flush any in-progress audio and emit a final `completed`.
//!   No-op if VAD already auto-committed the trailing audio.
//! - Receive: `conversation.item.input_audio_transcription.delta` (live
//!   incremental text; append-only for `gpt-4o-(mini-)transcribe` and
//!   `gpt-realtime-whisper`; for `whisper-1` the delta carries the full
//!   final text in one shot, no real streaming) and
//!   `conversation.item.input_audio_transcription.completed` (polished
//!   final). With VAD, MULTIPLE delta+completed cycles may land per
//!   session as utterances accumulate.
//!
//! Targets OpenAI cloud. OpenAI-compatible local realtime servers
//! (Lemonade etc.) need to implement the same wire shape to work with
//! this client.

use std::time::Duration;

use base64::Engine;
use base64::engine::general_purpose::STANDARD as B64;
use futures_util::{SinkExt, StreamExt};
use serde::Deserialize;
use serde_json::{Value, json};
use tokio::sync::mpsc;
use tokio_tungstenite::tungstenite::Message;
use tokio_tungstenite::tungstenite::client::IntoClientRequest;
use tracing::{info, warn};

use super::{ResolvedTranscriptionConfig, TranscriptionError};

/// Sample rate the OpenAI Realtime transcription API expects for PCM16
/// audio. Lemonade Server v9.4.1+ accepts the same. The capture pipeline
/// on the C++ side resamples (or captures natively) to this rate.
pub const REALTIME_SAMPLE_RATE_HZ: u32 = 24000;

/// Server-VAD silence window. Short enough that brief between-phrase
/// pauses trigger transcription updates so the user sees text appear
/// roughly phrase-by-phrase, while still long enough that mid-word
/// hesitations don't fragment the transcript. 500ms matches OpenAI's
/// recommended default.
///
/// Only applies to VAD-based models. Audio is buffered until VAD
/// detects silence (or the client sends commit), then the model
/// transcribes and emits deltas, so deltas flow per-utterance, not
/// per-word. Native-streaming models (`gpt-realtime-whisper`) bypass
/// `turn_detection` entirely.
const VAD_SILENCE_DURATION_MS: u32 = 500;

/// Hard cap on a single realtime session's wall-clock duration. Mirrors
/// the batch module's recording cap to prevent a stuck Space-hold from
/// streaming audio forever.
const SESSION_WALLCLOCK_CAP: Duration = Duration::from_secs(10 * 60);

/// Pre-commit idle timeout: if we receive nothing from the server while
/// the user is still recording, something is wrong (network stall, server
/// crash). Surface a network error rather than hanging the banner.
const PRE_COMMIT_IDLE_TIMEOUT: Duration = Duration::from_secs(30);

/// Post-commit idle timeout: after `input_audio_buffer.commit` is sent we
/// wait for the server to emit any pending `completed` event. If VAD
/// already finalised the trailing utterance there's nothing to wait for,
/// so this is short — we close cleanly on timeout rather than treating
/// it as an error.
const POST_COMMIT_RECV_TIMEOUT: Duration = Duration::from_secs(3);

/// Control messages from the C++ side into the running session task.
#[derive(Debug)]
pub enum ControlMessage {
    /// Raw little-endian PCM16 bytes captured from the mic. The session
    /// task base64-encodes and forwards these as `input_audio_buffer.append`.
    Audio(Vec<u8>),
    /// User released Space / clicked Stop. Send `input_audio_buffer.commit`
    /// and wait for the polished `completed` event.
    Commit,
    /// User pressed Esc / dismissed the banner. Tear down the session
    /// without waiting for any further events.
    Cancel,
}

/// Events emitted by the session task into the per-job queue. Drained by
/// the C++ side on a Qt timer.
#[derive(Debug, Clone)]
pub enum SessionEvent {
    /// Incremental text for the in-progress utterance. The composer
    /// renders this as a tentative (italic / muted) range that grows
    /// with each delta.
    Delta(String),
    /// Polished final transcript for ONE utterance. Multiple `Completed`
    /// events can land in a single session when server VAD splits on
    /// detected silence. The composer replaces the current tentative
    /// range with this polished text and starts a fresh range for any
    /// subsequent deltas.
    Completed(String),
    /// Session ended in failure. The composer surfaces the error in the
    /// banner.
    Failed(TranscriptionError),
    /// Session ended cleanly. Always emitted as the last event of a
    /// successful session (after any final `Completed`). The composer
    /// uses this to clean up any leftover tentative range and reset
    /// state.
    Closed,
}

/// The async session task. Connects, configures, then loops on the
/// control channel + WS stream. Pushes events into `event_tx` until the
/// session ends (commit + completed, cancel, or error). Returns once the
/// session is fully torn down.
pub async fn run_session(
    cfg: ResolvedTranscriptionConfig,
    mut control_rx: mpsc::UnboundedReceiver<ControlMessage>,
    event_tx: mpsc::UnboundedSender<SessionEvent>,
) {
    info!(
        target: "net",
        api_url = %cfg.api_url,
        model = %cfg.model,
        language = %cfg.language,
        prompt_len = cfg.prompt.len(),
        has_api_key = cfg.api_key.as_deref().is_some_and(|s| !s.is_empty()),
        sample_rate = REALTIME_SAMPLE_RATE_HZ,
        "starting realtime transcription session"
    );

    if cfg.api_url.trim().is_empty() {
        warn!(target: "net", "api_url is empty; aborting");
        let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::not_configured(
            "transcription api_url is not configured",
        )));
        return;
    }
    if cfg.model.trim().is_empty() {
        warn!(target: "net", "model is empty; aborting");
        let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::not_configured(
            "transcription model is not configured",
        )));
        return;
    }

    let ws_url = match build_ws_url(&cfg.api_url) {
        Ok(url) => url,
        Err(err) => {
            warn!(
                target: "net",
                api_url = %cfg.api_url,
                error = %err,
                "could not derive websocket url"
            );
            let _ = event_tx.send(SessionEvent::Failed(err));
            return;
        }
    };
    info!(
        target: "net",
        ws_url = %ws_url,
        "connecting to realtime transcription websocket"
    );

    let mut request = match (&ws_url).into_client_request() {
        Ok(req) => req,
        Err(e) => {
            warn!(
                target: "net",
                ws_url = %ws_url,
                error = %e,
                "could not build websocket request"
            );
            let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::network(format!(
                "could not build websocket request for {ws_url}: {e}"
            ))));
            return;
        }
    };
    {
        let headers = request.headers_mut();
        if let Some(api_key) = cfg.api_key.as_deref().filter(|s| !s.is_empty())
            && let Ok(value) = format!("Bearer {api_key}").parse() {
                headers.insert("Authorization", value);
            }
    }

    let connect = tokio::time::timeout(
        Duration::from_secs(15),
        tokio_tungstenite::connect_async(request),
    )
    .await;

    let ws_stream = match connect {
        Ok(Ok((stream, resp))) => {
            info!(
                target: "net",
                ws_url = %ws_url,
                status = %resp.status(),
                "websocket connected"
            );
            stream
        }
        Ok(Err(e)) => {
            warn!(
                target: "net",
                ws_url = %ws_url,
                error = %e,
                "websocket handshake failed"
            );
            let _ = event_tx.send(SessionEvent::Failed(map_ws_error(&ws_url, e)));
            return;
        }
        Err(_) => {
            warn!(
                target: "net",
                ws_url = %ws_url,
                "websocket connection timed out"
            );
            let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::network(format!(
                "websocket connection to {ws_url} timed out"
            ))));
            return;
        }
    };

    let (mut ws_tx, mut ws_rx) = ws_stream.split();

    // Initial session.update.
    let session_update = build_session_update(&cfg);
    info!(
        target: "net",
        payload = %session_update,
        "sending session.update"
    );
    if let Err(e) = ws_tx
        .send(Message::Text(session_update.to_string().into()))
        .await
    {
        warn!(
            target: "net",
            error = %e,
            "could not send session.update"
        );
        let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::network(format!(
            "could not send session.update: {e}"
        ))));
        return;
    }

    let session_started = std::time::Instant::now();
    let mut commit_sent = false;
    let mut completed_after_commit = false;
    let mut ended_for_failure = false;
    let mut audio_chunks_sent: u64 = 0;
    let mut audio_bytes_sent: u64 = 0;

    loop {
        if session_started.elapsed() > SESSION_WALLCLOCK_CAP {
            let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::invalid_audio(
                "recording too long — released after wall-clock cap",
            )));
            ended_for_failure = true;
            break;
        }
        // Once the user has committed AND we've received the
        // post-commit completed (or the post-commit timeout has fired in
        // the recv branch below) the session is done.
        if completed_after_commit {
            break;
        }

        let recv_timeout = if commit_sent {
            POST_COMMIT_RECV_TIMEOUT
        } else {
            PRE_COMMIT_IDLE_TIMEOUT
        };
        tokio::select! {
            biased;
            ctrl = control_rx.recv() => {
                match ctrl {
                    Some(ControlMessage::Audio(bytes)) => {
                        if commit_sent { continue; }
                        let payload = json!({
                            "type": "input_audio_buffer.append",
                            "audio": B64.encode(&bytes),
                        });
                        if let Err(e) = ws_tx.send(Message::Text(payload.to_string().into())).await {
                            warn!(
                                target: "net",
                                error = %e,
                                chunks_sent = audio_chunks_sent,
                                bytes_sent = audio_bytes_sent,
                                "could not send audio chunk"
                            );
                            let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::network(
                                format!("could not send audio chunk: {e}"))));
                            ended_for_failure = true;
                            break;
                        }
                        audio_chunks_sent += 1;
                        audio_bytes_sent += bytes.len() as u64;
                    }
                    Some(ControlMessage::Commit) => {
                        if commit_sent { continue; }
                        info!(
                            target: "net",
                            chunks_sent = audio_chunks_sent,
                            bytes_sent = audio_bytes_sent,
                            "sending input_audio_buffer.commit"
                        );
                        let payload = json!({"type": "input_audio_buffer.commit"});
                        if let Err(e) = ws_tx.send(Message::Text(payload.to_string().into())).await {
                            warn!(
                                target: "net",
                                error = %e,
                                "could not send commit"
                            );
                            let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::network(
                                format!("could not send commit: {e}"))));
                            ended_for_failure = true;
                            break;
                        }
                        commit_sent = true;
                    }
                    Some(ControlMessage::Cancel) | None => {
                        info!(
                            target: "net",
                            chunks_sent = audio_chunks_sent,
                            bytes_sent = audio_bytes_sent,
                            commit_sent,
                            "session cancelled"
                        );
                        // Best-effort close; we don't care about the result.
                        let _ = ws_tx.send(Message::Close(None)).await;
                        return;
                    }
                }
            }
            recv = tokio::time::timeout(recv_timeout, ws_rx.next()) => {
                match recv {
                    Err(_) => {
                        if commit_sent {
                            // Post-commit timeout: VAD likely already
                            // finalised the trailing audio so the server
                            // has nothing to send. Close cleanly.
                            info!(
                                target: "net",
                                "post-commit idle timeout; closing session cleanly"
                            );
                            break;
                        }
                        warn!(
                            target: "net",
                            "no response from server within idle timeout"
                        );
                        let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::network(
                            "no response from transcription server",
                        )));
                        ended_for_failure = true;
                        break;
                    }
                    Ok(None) => {
                        if !commit_sent {
                            warn!(
                                target: "net",
                                "websocket closed before commit"
                            );
                            let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::network(
                                "transcription websocket closed unexpectedly",
                            )));
                            ended_for_failure = true;
                        }
                        break;
                    }
                    Ok(Some(Err(e))) => {
                        warn!(
                            target: "net",
                            error = %e,
                            "websocket stream error"
                        );
                        let _ = event_tx.send(SessionEvent::Failed(map_ws_error(&ws_url, e)));
                        ended_for_failure = true;
                        break;
                    }
                    Ok(Some(Ok(Message::Text(text)))) => {
                        match handle_server_event(&text, &event_tx) {
                            ServerEventOutcome::Completed => {
                                if commit_sent {
                                    completed_after_commit = true;
                                }
                                // Otherwise this completed came from VAD
                                // mid-recording; keep the loop alive so
                                // subsequent deltas keep flowing.
                            }
                            ServerEventOutcome::Continue => {}
                            ServerEventOutcome::CommitNoOp => {
                                // Server VAD already finalised everything.
                                // No more transcripts coming, close cleanly.
                                break;
                            }
                            ServerEventOutcome::Failed => {
                                ended_for_failure = true;
                                break;
                            }
                        }
                    }
                    Ok(Some(Ok(Message::Binary(_)))) => {
                        // Realtime API does not use binary frames for transcription;
                        // ignore defensively.
                    }
                    Ok(Some(Ok(Message::Ping(payload)))) => {
                        let _ = ws_tx.send(Message::Pong(payload)).await;
                    }
                    Ok(Some(Ok(Message::Pong(_) | Message::Frame(_)))) => {}
                    Ok(Some(Ok(Message::Close(_)))) => {
                        if !commit_sent {
                            // Pre-commit close = something went wrong on
                            // the server side. Post-commit close is just
                            // the server tearing down after the final
                            // utterance, which is success.
                            let _ = event_tx.send(SessionEvent::Failed(TranscriptionError::network(
                                "transcription websocket closed before commit",
                            )));
                            ended_for_failure = true;
                        }
                        break;
                    }
                }
            }
        }
    }

    let _ = ws_tx.send(Message::Close(None)).await;
    if ended_for_failure {
        // Drain the inbound side briefly so the close handshake completes
        // cleanly. Best-effort.
        let _ = tokio::time::timeout(Duration::from_millis(200), async {
            while ws_rx.next().await.is_some() {}
        })
        .await;
    } else {
        // Successful end of session — let the composer know so it can
        // clean up any leftover tentative range and reset state. After a
        // failure we deliberately skip Closed: SessionEvent::Failed has
        // already moved the composer to the error state and an extra
        // Closed there would needlessly push the banner back to idle.
        let _ = event_tx.send(SessionEvent::Closed);
    }
    info!(
        target: "net",
        chunks_sent = audio_chunks_sent,
        bytes_sent = audio_bytes_sent,
        commit_sent,
        ended_for_failure,
        "realtime session task exiting"
    );
}

enum ServerEventOutcome {
    Continue,
    Completed,
    /// The explicit `input_audio_buffer.commit` we sent landed on an
    /// empty buffer because server VAD already auto-committed every
    /// utterance. Not actually an error; the session is just done.
    CommitNoOp,
    Failed,
}

/// Parse one inbound JSON event and translate it into [`SessionEvent`]s
/// pushed into `event_tx`. Returns whether the session should end.
fn handle_server_event(
    text: &str,
    event_tx: &mpsc::UnboundedSender<SessionEvent>,
) -> ServerEventOutcome {
    #[derive(Deserialize)]
    struct Envelope {
        #[serde(rename = "type")]
        ty: String,
        #[serde(default)]
        delta: Option<String>,
        #[serde(default)]
        transcript: Option<String>,
        #[serde(default)]
        error: Option<ErrorPayload>,
    }
    #[derive(Deserialize)]
    struct ErrorPayload {
        #[serde(default)]
        message: Option<String>,
        #[serde(default)]
        code: Option<String>,
    }

    let parsed: Envelope = match serde_json::from_str(text) {
        Ok(v) => v,
        Err(e) => {
            // Unknown shapes are ignored — the server may emit informational
            // events we don't care about (session.created, session.updated,
            // input_audio_buffer.committed, …). Log the body so we can spot
            // genuine protocol mismatches in the wild.
            warn!(
                target: "net",
                error = %e,
                body = %truncate_for_log(text, 256),
                "could not parse server event"
            );
            return ServerEventOutcome::Continue;
        }
    };

    match parsed.ty.as_str() {
        "conversation.item.input_audio_transcription.delta" => {
            // Don't log per delta — they fire at typing speed and drown
            // the log. The cumulative count + bytes of audio sent is
            // enough to spot "audio went out, transcripts came back" at
            // session-end time.
            if let Some(d) = parsed.delta
                && !d.is_empty()
            {
                let _ = event_tx.send(SessionEvent::Delta(d));
            }
            ServerEventOutcome::Continue
        }
        "conversation.item.input_audio_transcription.completed" => {
            let final_text = parsed.transcript.unwrap_or_default();
            info!(
                target: "net",
                transcript_len = final_text.len(),
                "completed utterance"
            );
            let _ = event_tx.send(SessionEvent::Completed(final_text));
            ServerEventOutcome::Completed
        }
        "conversation.item.input_audio_transcription.failed" | "error" => {
            let (code, message) = match parsed.error {
                Some(payload) => (
                    payload.code.unwrap_or_default(),
                    payload.message.unwrap_or_else(|| "unknown error".to_owned()),
                ),
                None => (String::new(), "unknown error".to_owned()),
            };
            // `input_audio_buffer_commit_empty` is benign: server VAD
            // already auto-committed every utterance, so by the time the
            // user releases Space and we send our explicit commit, the
            // buffer is empty. Treat as "session done, close cleanly"
            // rather than surfacing an error banner.
            if code.eq_ignore_ascii_case("input_audio_buffer_commit_empty") {
                info!(
                    target: "net",
                    "explicit commit found no pending audio (VAD already flushed); closing cleanly"
                );
                return ServerEventOutcome::CommitNoOp;
            }
            warn!(
                target: "net",
                event_type = %parsed.ty,
                code = %code,
                message = %message,
                raw_body = %truncate_for_log(text, 512),
                "server reported error"
            );
            let err = if code.eq_ignore_ascii_case("invalid_api_key")
                || code.eq_ignore_ascii_case("unauthorized")
                || message.to_ascii_lowercase().contains("api key")
            {
                TranscriptionError::unauthorized(message)
            } else {
                TranscriptionError::server_error(message)
            };
            let _ = event_tx.send(SessionEvent::Failed(err));
            ServerEventOutcome::Failed
        }
        _ => ServerEventOutcome::Continue,
    }
}

fn truncate_for_log(s: &str, max_chars: usize) -> String {
    if s.chars().count() <= max_chars {
        return s.to_owned();
    }
    let mut truncated: String = s.chars().take(max_chars).collect();
    truncated.push('…');
    truncated
}

fn build_session_update(cfg: &ResolvedTranscriptionConfig) -> Value {
    let mut transcription = serde_json::Map::new();
    transcription.insert("model".into(), Value::String(cfg.model.clone()));
    if !cfg.language.trim().is_empty() && cfg.language != "auto" {
        transcription.insert("language".into(), Value::String(cfg.language.clone()));
    }
    if !cfg.prompt.trim().is_empty() {
        transcription.insert("prompt".into(), Value::String(cfg.prompt.clone()));
    }

    let mut input = serde_json::Map::new();
    input.insert(
        "format".into(),
        json!({"type": "audio/pcm", "rate": REALTIME_SAMPLE_RATE_HZ}),
    );
    input.insert("transcription".into(), Value::Object(transcription));
    if model_supports_turn_detection(&cfg.model) {
        // Phrase-by-phrase streaming: server VAD splits utterances on
        // detected silence so deltas land while the user is still
        // speaking. Models that stream natively (see
        // `model_supports_turn_detection`) reject this block.
        input.insert(
            "turn_detection".into(),
            json!({
                "type": "server_vad",
                "threshold": 0.5,
                "prefix_padding_ms": 300,
                "silence_duration_ms": VAD_SILENCE_DURATION_MS,
            }),
        );
    }

    // `session.type: "transcription"` is required to distinguish from S2S.
    json!({
        "type": "session.update",
        "session": {
            "type": "transcription",
            "audio": {
                "input": Value::Object(input),
            },
        }
    })
}

/// Some realtime transcription models stream natively and reject the
/// `turn_detection` block (`invalid_value: Turn detection is not
/// supported for this transcription model`). They pace deltas
/// themselves rather than waiting on server-VAD-detected silence.
fn model_supports_turn_detection(model: &str) -> bool {
    !matches!(model, "gpt-realtime-whisper")
}

/// Derive the WebSocket URL for the OpenAI Realtime transcription
/// endpoint from the user-configured HTTP base. `?intent=transcription`
/// anchors the connection to the transcription session category; the
/// transcription model is configured separately in the
/// `session.update` payload sent immediately after connect.
///
/// Examples:
/// - `https://api.openai.com/v1` → `wss://api.openai.com/v1/realtime?intent=transcription`
/// - `http://localhost:8000/v1`  → `ws://localhost:8000/v1/realtime?intent=transcription`
/// - `wss://example.com/v1/realtime` → with `?intent=transcription` appended.
fn build_ws_url(api_url: &str) -> Result<String, TranscriptionError> {
    let trimmed = api_url.trim().trim_end_matches('/');
    if trimmed.is_empty() {
        return Err(TranscriptionError::not_configured("api_url is empty"));
    }

    let (base, existing_query) = if let Some(rest) = trimmed.strip_prefix("https://") {
        (with_realtime_suffix(rest, "wss://"), None)
    } else if let Some(rest) = trimmed.strip_prefix("http://") {
        (with_realtime_suffix(rest, "ws://"), None)
    } else if trimmed.starts_with("wss://") || trimmed.starts_with("ws://") {
        let (head, query) = match trimmed.split_once('?') {
            Some((h, q)) => (h, Some(q)),
            None => (trimmed, None),
        };
        let head_with_realtime = if head.ends_with("/realtime") {
            head.to_owned()
        } else {
            format!("{head}/realtime")
        };
        (head_with_realtime, query)
    } else {
        return Err(TranscriptionError::not_configured(format!(
            "api_url {api_url:?} must start with http://, https://, ws:// or wss://"
        )));
    };

    Ok(match existing_query {
        Some(q) if q.contains("intent=") => format!("{base}?{q}"),
        Some(q) if !q.is_empty() => format!("{base}?{q}&intent=transcription"),
        _ => format!("{base}?intent=transcription"),
    })
}

fn with_realtime_suffix(rest: &str, scheme: &str) -> String {
    if rest.ends_with("/realtime") {
        format!("{scheme}{rest}")
    } else {
        format!("{scheme}{rest}/realtime")
    }
}

fn map_ws_error(url: &str, err: tokio_tungstenite::tungstenite::Error) -> TranscriptionError {
    use tokio_tungstenite::tungstenite::Error as E;
    use tokio_tungstenite::tungstenite::http::StatusCode;
    match err {
        E::Http(resp) => {
            let status = resp.status();
            let body = resp
                .body()
                .as_ref()
                .map(|b| String::from_utf8_lossy(b).into_owned())
                .unwrap_or_default();
            let mut detail = if body.is_empty() {
                format!("HTTP {status}")
            } else {
                let mut snippet = body;
                snippet.truncate(256);
                format!("HTTP {status}: {snippet}")
            };
            if detail.is_empty() {
                detail = format!("websocket handshake failed for {url}");
            }
            if status == StatusCode::UNAUTHORIZED || status == StatusCode::FORBIDDEN {
                TranscriptionError::unauthorized(detail)
            } else {
                TranscriptionError::server_error(detail)
            }
        }
        E::Url(e) => TranscriptionError::not_configured(format!("invalid websocket url {url}: {e}")),
        E::Io(e) => TranscriptionError::network(format!("websocket I/O error for {url}: {e}")),
        E::Tls(e) => TranscriptionError::network(format!("websocket TLS error for {url}: {e}")),
        E::ConnectionClosed | E::AlreadyClosed => {
            TranscriptionError::network(format!("websocket connection to {url} closed"))
        }
        other => TranscriptionError::network(format!("websocket error for {url}: {other}")),
    }
}

#[cfg(test)]
mod tests;
