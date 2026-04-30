// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! FFI surface for the transcription module.
//!
//! C++ uses these to:
//! - Resolve effective per-room transcription config (so the UI can decide
//!   whether long-press Space activates recording or shows the configure-me
//!   hint).
//! - Run a batch transcription against an audio file.
//! - Read/write/clear api keys at the global or per-room level.

use std::collections::{HashMap, VecDeque};
use std::path::Path;
use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::{Arc, Mutex, OnceLock};

use tokio::runtime::Runtime;
use tokio::sync::mpsc;

use crate::ffi::{
    SettingsOptionalString, TranscriptionBatchResult, TranscriptionErrorCodeFfi,
    TranscriptionRealtimeEvent, TranscriptionRealtimeEventKindFfi,
    TranscriptionRealtimeStartResult, TranscriptionResolvedConfig,
};
use crate::settings::{config as settings_config, storage};
use crate::transcription::{
    TranscriptionErrorCode, batch, config::TranscriptionProvider, realtime, secrets,
};

/// Process-wide tokio runtime for the transcription HTTP work. Kept local so
/// the transcription module doesn't have to share the matrix-sdk runtime.
fn runtime() -> &'static Runtime {
    static RT: OnceLock<Runtime> = OnceLock::new();
    RT.get_or_init(|| {
        tokio::runtime::Builder::new_multi_thread()
            .enable_all()
            .worker_threads(2)
            .thread_name("komai-transcription")
            .build()
            .expect("failed to build transcription tokio runtime")
    })
}

/// Read & parse the on-disk `config.yml` for a profile. Convenience wrapper
/// for the FFI helpers below — none of these are hot-path so re-reading the
/// file each call is fine.
fn load_parsed_config(profile_id: &str) -> settings_config::Config {
    let config_path = storage::config_file_path_for_profile(profile_id);
    let config_text = storage::read_text_file(&config_path, "config");
    settings_config::parse_config_text(&config_text)
}

fn provider_to_storage_string(provider: TranscriptionProvider) -> String {
    use settings_config::ConfigIntegrationsTranscriptionProviderToken as Token;
    let token = match provider {
        TranscriptionProvider::OpenaiBatch => Token::OpenaiBatch,
        TranscriptionProvider::OpenaiRealtime => Token::OpenaiRealtime,
    };
    token.to_storage_string()
}

fn error_code_to_ffi(code: TranscriptionErrorCode) -> TranscriptionErrorCodeFfi {
    match code {
        TranscriptionErrorCode::NotConfigured => TranscriptionErrorCodeFfi::NotConfigured,
        TranscriptionErrorCode::Network => TranscriptionErrorCodeFfi::Network,
        TranscriptionErrorCode::Unauthorized => TranscriptionErrorCodeFfi::Unauthorized,
        TranscriptionErrorCode::ServerError => TranscriptionErrorCodeFfi::ServerError,
        TranscriptionErrorCode::InvalidResponse => TranscriptionErrorCodeFfi::InvalidResponse,
        TranscriptionErrorCode::InvalidAudio => TranscriptionErrorCodeFfi::InvalidAudio,
        TranscriptionErrorCode::Internal => TranscriptionErrorCodeFfi::Internal,
    }
}

fn optional_string(value: Option<String>) -> SettingsOptionalString {
    match value {
        Some(value) => SettingsOptionalString {
            has_value: true,
            value,
        },
        None => SettingsOptionalString {
            has_value: false,
            value: String::new(),
        },
    }
}

/// Returns the fully resolved (global + per-room override + secrets) config
/// for a given room. The UI uses this to decide whether the feature is ready
/// to use.
pub(crate) fn transcription_resolve_for_room(
    profile_id: &str,
    room_id: &str,
) -> TranscriptionResolvedConfig {
    let parsed = load_parsed_config(profile_id);
    let global_api_key = secrets::load_global_api_key(profile_id);
    let room_api_key = if room_id.is_empty() {
        None
    } else {
        secrets::load_room_api_key(profile_id, room_id)
    };
    let resolved = crate::transcription::config::resolve_for_room(
        &parsed.integrations.transcription,
        room_id,
        global_api_key,
        room_api_key,
    );

    let needs_api_key =
        crate::transcription::config::ResolvedTranscriptionConfig::url_likely_requires_api_key(
            &resolved.api_url,
        );
    let is_ready = resolved.is_ready(needs_api_key);

    TranscriptionResolvedConfig {
        provider: provider_to_storage_string(resolved.provider),
        api_url: resolved.api_url,
        has_api_key: resolved.api_key.as_deref().is_some_and(|s| !s.is_empty()),
        needs_api_key,
        model: resolved.model,
        language: resolved.language,
        prompt: resolved.prompt,
        is_ready,
    }
}

pub(crate) fn transcription_run_batch(
    profile_id: &str,
    room_id: &str,
    audio_path: &str,
) -> TranscriptionBatchResult {
    let parsed = load_parsed_config(profile_id);
    let global_api_key = secrets::load_global_api_key(profile_id);
    let room_api_key = if room_id.is_empty() {
        None
    } else {
        secrets::load_room_api_key(profile_id, room_id)
    };
    let resolved = crate::transcription::config::resolve_for_room(
        &parsed.integrations.transcription,
        room_id,
        global_api_key,
        room_api_key,
    );

    let path = Path::new(audio_path);
    let outcome = runtime().block_on(batch::transcribe_file(path, &resolved));

    match outcome {
        Ok(text) => TranscriptionBatchResult {
            success: true,
            text,
            error_code: TranscriptionErrorCodeFfi::Ok,
            error_message: String::new(),
        },
        Err(err) => TranscriptionBatchResult {
            success: false,
            text: String::new(),
            error_code: error_code_to_ffi(err.code),
            error_message: err.message,
        },
    }
}

pub(crate) fn transcription_load_global_api_key(profile_id: &str) -> SettingsOptionalString {
    optional_string(secrets::load_global_api_key(profile_id))
}

pub(crate) fn transcription_save_global_api_key(profile_id: &str, value: &str) {
    secrets::save_global_api_key(profile_id, Some(value));
}

pub(crate) fn transcription_clear_global_api_key(profile_id: &str) {
    secrets::save_global_api_key(profile_id, None);
}

pub(crate) fn transcription_load_room_api_key(
    profile_id: &str,
    room_id: &str,
) -> SettingsOptionalString {
    if room_id.is_empty() {
        return optional_string(None);
    }
    optional_string(secrets::load_room_api_key(profile_id, room_id))
}

pub(crate) fn transcription_save_room_api_key(profile_id: &str, room_id: &str, value: &str) {
    if room_id.is_empty() {
        return;
    }
    secrets::save_room_api_key(profile_id, room_id, Some(value));
}

pub(crate) fn transcription_clear_room_api_key(profile_id: &str, room_id: &str) {
    if room_id.is_empty() {
        return;
    }
    secrets::save_room_api_key(profile_id, room_id, None);
}

// --- Realtime / streaming transcription ------------------------------------
//
// The session task runs in our local tokio runtime. The C++ side drives
// it through a small handle table keyed by `job_id`: push audio chunks,
// commit on release, cancel on Esc, drain events on a Qt timer. We do
// not expose the underlying tokio types to C++ — only `i64` job ids.

struct RealtimeJob {
    control_tx: mpsc::UnboundedSender<realtime::ControlMessage>,
    /// Queue of events the C++ side hasn't drained yet.
    events: Mutex<VecDeque<realtime::SessionEvent>>,
    /// Set once the session task has emitted its terminal event (Completed
    /// or Failed). After draining, the C++ side can drop the job.
    terminal_seen: std::sync::atomic::AtomicBool,
}

fn realtime_jobs() -> &'static Mutex<HashMap<i64, Arc<RealtimeJob>>> {
    static MAP: OnceLock<Mutex<HashMap<i64, Arc<RealtimeJob>>>> = OnceLock::new();
    MAP.get_or_init(|| Mutex::new(HashMap::new()))
}

fn next_realtime_job_id() -> i64 {
    static NEXT: AtomicI64 = AtomicI64::new(1);
    NEXT.fetch_add(1, Ordering::Relaxed)
}

fn event_kind_to_ffi(event: &realtime::SessionEvent) -> TranscriptionRealtimeEvent {
    match event {
        realtime::SessionEvent::Delta(text) => TranscriptionRealtimeEvent {
            kind: TranscriptionRealtimeEventKindFfi::Delta,
            text: text.clone(),
            error_code: TranscriptionErrorCodeFfi::Ok,
            error_message: String::new(),
        },
        realtime::SessionEvent::Completed(text) => TranscriptionRealtimeEvent {
            kind: TranscriptionRealtimeEventKindFfi::Completed,
            text: text.clone(),
            error_code: TranscriptionErrorCodeFfi::Ok,
            error_message: String::new(),
        },
        realtime::SessionEvent::Failed(err) => TranscriptionRealtimeEvent {
            kind: TranscriptionRealtimeEventKindFfi::Failed,
            text: String::new(),
            error_code: error_code_to_ffi(err.code),
            error_message: err.message.clone(),
        },
        realtime::SessionEvent::Closed => TranscriptionRealtimeEvent {
            kind: TranscriptionRealtimeEventKindFfi::Closed,
            text: String::new(),
            error_code: TranscriptionErrorCodeFfi::Ok,
            error_message: String::new(),
        },
    }
}

pub(crate) fn transcription_realtime_sample_rate_hz() -> u32 {
    realtime::REALTIME_SAMPLE_RATE_HZ
}

pub(crate) fn transcription_run_realtime(
    profile_id: &str,
    room_id: &str,
) -> TranscriptionRealtimeStartResult {
    let parsed = load_parsed_config(profile_id);
    let global_api_key = secrets::load_global_api_key(profile_id);
    let room_api_key = if room_id.is_empty() {
        None
    } else {
        secrets::load_room_api_key(profile_id, room_id)
    };
    let resolved = crate::transcription::config::resolve_for_room(
        &parsed.integrations.transcription,
        room_id,
        global_api_key,
        room_api_key,
    );

    let needs_api_key =
        crate::transcription::config::ResolvedTranscriptionConfig::url_likely_requires_api_key(
            &resolved.api_url,
        );
    if !resolved.is_ready(needs_api_key) {
        return TranscriptionRealtimeStartResult {
            job_id: 0,
            accepted: false,
            error_code: TranscriptionErrorCodeFfi::NotConfigured,
            error_message: "transcription is not configured for this room".to_owned(),
        };
    }

    let (control_tx, control_rx) = mpsc::unbounded_channel();
    let (event_tx, mut event_rx) = mpsc::unbounded_channel();

    let job_id = next_realtime_job_id();
    let job = Arc::new(RealtimeJob {
        control_tx,
        events: Mutex::new(VecDeque::new()),
        terminal_seen: std::sync::atomic::AtomicBool::new(false),
    });
    realtime_jobs()
        .lock()
        .expect("realtime jobs map poisoned")
        .insert(job_id, job.clone());

    // Spawn the session task.
    runtime().spawn(async move {
        realtime::run_session(resolved, control_rx, event_tx).await;
    });

    // Spawn a forwarder that drains the per-session event channel into
    // the C++-visible queue. This decouples the network task from the
    // C++ poll cadence, and is the natural place to set the terminal
    // flag once the session ends.
    //
    // `Completed` is NOT terminal — server VAD can emit several of them
    // per session (one per detected utterance) and the composer keeps
    // streaming deltas afterwards. Only `Failed` and `Closed` end the
    // session.
    {
        let job = job.clone();
        runtime().spawn(async move {
            while let Some(event) = event_rx.recv().await {
                let is_terminal = matches!(
                    event,
                    realtime::SessionEvent::Failed(_) | realtime::SessionEvent::Closed
                );
                if let Ok(mut queue) = job.events.lock() {
                    queue.push_back(event);
                }
                if is_terminal {
                    job.terminal_seen
                        .store(true, std::sync::atomic::Ordering::Relaxed);
                }
            }
            // Channel closed without a terminal event (the session task
            // dropped its sender abruptly). Mark terminal so C++ stops
            // polling.
            job.terminal_seen
                .store(true, std::sync::atomic::Ordering::Relaxed);
        });
    }

    TranscriptionRealtimeStartResult {
        job_id,
        accepted: true,
        error_code: TranscriptionErrorCodeFfi::Ok,
        error_message: String::new(),
    }
}

pub(crate) fn transcription_realtime_push_audio(job_id: i64, pcm16_bytes: &[u8]) {
    if pcm16_bytes.is_empty() {
        return;
    }
    let job = match realtime_jobs()
        .lock()
        .expect("realtime jobs map poisoned")
        .get(&job_id)
        .cloned()
    {
        Some(j) => j,
        None => return,
    };
    // Best-effort: if the receiver has been dropped (session ended),
    // silently swallow. The C++ side will see the terminal event in the
    // next drain.
    let _ = job
        .control_tx
        .send(realtime::ControlMessage::Audio(pcm16_bytes.to_vec()));
}

pub(crate) fn transcription_realtime_commit(job_id: i64) {
    let job = match realtime_jobs()
        .lock()
        .expect("realtime jobs map poisoned")
        .get(&job_id)
        .cloned()
    {
        Some(j) => j,
        None => return,
    };
    let _ = job.control_tx.send(realtime::ControlMessage::Commit);
}

pub(crate) fn transcription_realtime_cancel(job_id: i64) {
    // Remove the job from the registry first so subsequent push/commit
    // calls become no-ops, then signal the task to tear down. The forwarder
    // task will set terminal_seen once the event channel closes.
    let job = realtime_jobs()
        .lock()
        .expect("realtime jobs map poisoned")
        .remove(&job_id);
    if let Some(job) = job {
        let _ = job.control_tx.send(realtime::ControlMessage::Cancel);
    }
}

pub(crate) fn transcription_realtime_drain_events(job_id: i64) -> Vec<TranscriptionRealtimeEvent> {
    let job = match realtime_jobs()
        .lock()
        .expect("realtime jobs map poisoned")
        .get(&job_id)
        .cloned()
    {
        Some(j) => j,
        None => return Vec::new(),
    };

    let drained: Vec<realtime::SessionEvent> = {
        let mut queue = match job.events.lock() {
            Ok(q) => q,
            Err(_) => return Vec::new(),
        };
        queue.drain(..).collect()
    };

    // If the session reached a terminal state and we just drained it,
    // remove it from the registry so resources are released.
    if job
        .terminal_seen
        .load(std::sync::atomic::Ordering::Relaxed)
    {
        let queue_empty = job
            .events
            .lock()
            .map(|q| q.is_empty())
            .unwrap_or(true);
        if queue_empty {
            realtime_jobs()
                .lock()
                .expect("realtime jobs map poisoned")
                .remove(&job_id);
        }
    }

    drained.iter().map(event_kind_to_ffi).collect()
}
