// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Batch transcription via the OpenAI-compatible
//! `POST {api_url}/audio/transcriptions` endpoint.
//!
//! Compatible with: OpenAI cloud, whisper.cpp `whisper-server` (with
//! `--inference-path /v1/audio/transcriptions`), AMD Lemonade Server,
//! LocalAI, vLLM. The `language` and `prompt` fields are passed through
//! verbatim; servers that don't honour them simply ignore them.

use std::path::Path;
use std::time::Duration;

use reqwest::multipart;
use serde::Deserialize;

use super::{ResolvedTranscriptionConfig, TranscriptionError};

/// Hard cap on a batch request, protecting against runaway recordings. The
/// OpenAI cloud limit is 25 MB; we stay well below that.
const MAX_AUDIO_BYTES: u64 = 25 * 1024 * 1024;

/// Minimum audio file size we'll bother sending. Catches the "user tapped
/// Space rather than long-pressed" case where the recorder produced a
/// near-empty file.
const MIN_AUDIO_BYTES: u64 = 4 * 1024;

/// Reasonable timeout for a batch request; long enough for a 5-minute
/// utterance to come back, short enough that a hung server doesn't lock the
/// composer indefinitely.
const REQUEST_TIMEOUT: Duration = Duration::from_secs(120);

#[derive(Debug, Deserialize)]
struct BatchResponse {
    text: String,
}

#[derive(Debug, Deserialize)]
struct ErrorEnvelope {
    error: ErrorDetail,
}

#[derive(Debug, Deserialize)]
struct ErrorDetail {
    message: String,
}

/// POST the audio file at `audio_path` and return the transcribed text.
///
/// Errors carry a [`TranscriptionErrorCode`](super::TranscriptionErrorCode)
/// the UI can branch on (e.g. surface an "Open Settings" hint for
/// `Unauthorized`). The `message` is suitable for surfacing directly in the
/// composer banner.
pub async fn transcribe_file(
    audio_path: &Path,
    cfg: &ResolvedTranscriptionConfig,
) -> Result<String, TranscriptionError> {
    if cfg.api_url.trim().is_empty() {
        return Err(TranscriptionError::not_configured(
            "transcription api_url is not configured",
        ));
    }
    if cfg.model.trim().is_empty() {
        return Err(TranscriptionError::not_configured(
            "transcription model is not configured",
        ));
    }

    let metadata = tokio::fs::metadata(audio_path).await.map_err(|e| {
        TranscriptionError::invalid_audio(format!(
            "could not read audio file {}: {}",
            audio_path.display(),
            e
        ))
    })?;
    let size = metadata.len();
    if size < MIN_AUDIO_BYTES {
        return Err(TranscriptionError::invalid_audio(
            "recording too short — try holding Space a bit longer",
        ));
    }
    if size > MAX_AUDIO_BYTES {
        return Err(TranscriptionError::invalid_audio(format!(
            "recording too long ({} bytes; cap is {} bytes)",
            size, MAX_AUDIO_BYTES,
        )));
    }

    let bytes = tokio::fs::read(audio_path).await.map_err(|e| {
        TranscriptionError::invalid_audio(format!(
            "could not read audio file {}: {}",
            audio_path.display(),
            e
        ))
    })?;

    let file_name = audio_path
        .file_name()
        .map(|f| f.to_string_lossy().into_owned())
        .unwrap_or_else(|| "audio.bin".to_owned());
    let mime = guess_mime(audio_path);

    let mut form = multipart::Form::new()
        .text("model", cfg.model.clone())
        .text("response_format", "json")
        .part(
            "file",
            multipart::Part::bytes(bytes)
                .file_name(file_name)
                .mime_str(mime)
                .map_err(|e| {
                    TranscriptionError::internal(format!("could not set mime type: {e}"))
                })?,
        );

    if !cfg.language.trim().is_empty() && cfg.language != "auto" {
        form = form.text("language", cfg.language.clone());
    }
    if !cfg.prompt.trim().is_empty() {
        form = form.text("prompt", cfg.prompt.clone());
    }

    let endpoint = build_endpoint(&cfg.api_url);

    let client = reqwest::Client::builder()
        .timeout(REQUEST_TIMEOUT)
        .build()
        .map_err(|e| TranscriptionError::internal(format!("http client build failed: {e}")))?;

    let mut request = client.post(&endpoint).multipart(form);
    if let Some(api_key) = cfg.api_key.as_deref().filter(|s| !s.is_empty()) {
        request = request.bearer_auth(api_key);
    }

    let response = request.send().await.map_err(|e| {
        // reqwest folds connect errors, dns errors, tls errors and timeouts
        // into a single `Error`. The `is_*` helpers let us pick a slightly
        // more specific code, but for the user a single "network" bucket
        // is fine.
        TranscriptionError::network(format!("request to {endpoint} failed: {e}"))
    })?;

    let status = response.status();
    if !status.is_success() {
        let body = response.text().await.unwrap_or_default();
        let detail = parse_error_message(&body).unwrap_or_else(|| {
            if body.is_empty() {
                format!("HTTP {status}")
            } else {
                let mut snippet = body;
                snippet.truncate(256);
                format!("HTTP {status}: {snippet}")
            }
        });
        return Err(if status == reqwest::StatusCode::UNAUTHORIZED
            || status == reqwest::StatusCode::FORBIDDEN
        {
            TranscriptionError::unauthorized(detail)
        } else {
            TranscriptionError::server_error(detail)
        });
    }

    let body = response.text().await.map_err(|e| {
        TranscriptionError::network(format!("could not read response body: {e}"))
    })?;

    let parsed: BatchResponse = serde_json::from_str(&body).map_err(|e| {
        let mut snippet = body.clone();
        snippet.truncate(256);
        TranscriptionError::invalid_response(format!(
            "could not parse server response as JSON: {e}; got: {snippet}"
        ))
    })?;

    Ok(parsed.text)
}

fn build_endpoint(api_url: &str) -> String {
    let trimmed = api_url.trim_end_matches('/');
    format!("{trimmed}/audio/transcriptions")
}

fn guess_mime(audio_path: &Path) -> &'static str {
    let ext = audio_path
        .extension()
        .and_then(|s| s.to_str())
        .map(|s| s.to_ascii_lowercase());
    match ext.as_deref() {
        Some("wav") => "audio/wav",
        Some("mp3") => "audio/mpeg",
        Some("m4a") | Some("mp4") => "audio/mp4",
        Some("ogg") | Some("oga") => "audio/ogg",
        Some("opus") => "audio/opus",
        Some("webm") => "audio/webm",
        Some("flac") => "audio/flac",
        // Default; OpenAI accepts the file based on contents, not Content-Type,
        // for this multipart form so this is mostly informational.
        _ => "application/octet-stream",
    }
}

fn parse_error_message(body: &str) -> Option<String> {
    serde_json::from_str::<ErrorEnvelope>(body)
        .ok()
        .map(|e| e.error.message)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn endpoint_is_appended_correctly() {
        assert_eq!(
            build_endpoint("https://api.openai.com/v1"),
            "https://api.openai.com/v1/audio/transcriptions"
        );
        assert_eq!(
            build_endpoint("https://api.openai.com/v1/"),
            "https://api.openai.com/v1/audio/transcriptions"
        );
        assert_eq!(
            build_endpoint("http://localhost:8080/v1///"),
            "http://localhost:8080/v1/audio/transcriptions"
        );
    }

    #[test]
    fn mime_guess_handles_common_cases() {
        assert_eq!(guess_mime(Path::new("foo.wav")), "audio/wav");
        assert_eq!(guess_mime(Path::new("foo.mp3")), "audio/mpeg");
        assert_eq!(guess_mime(Path::new("foo.M4A")), "audio/mp4");
        assert_eq!(guess_mime(Path::new("foo.unknown")), "application/octet-stream");
    }

    #[test]
    fn parse_error_envelope() {
        let body = r#"{"error":{"message":"Invalid API key","type":"auth"}}"#;
        assert_eq!(parse_error_message(body).as_deref(), Some("Invalid API key"));
        assert!(parse_error_message("not json").is_none());
    }
}
