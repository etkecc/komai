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

use std::path::Path;
use std::sync::OnceLock;

use tokio::runtime::Runtime;

use crate::ffi::{
    SettingsOptionalString, TranscriptionBatchResult, TranscriptionErrorCodeFfi,
    TranscriptionResolvedConfig,
};
use crate::settings::{config as settings_config, storage};
use crate::transcription::{
    TranscriptionErrorCode, batch, config::TranscriptionProvider, secrets,
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
