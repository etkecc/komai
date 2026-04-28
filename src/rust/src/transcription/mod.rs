// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Voice transcription support for the composer.
//!
//! High-level shape:
//!
//! - [`config`] resolves the effective per-room configuration from globals and
//!   per-room overrides parsed out of `integrations.transcription` plus api
//!   keys loaded from the secrets backend.
//! - [`secrets`] reads/writes api keys via the existing `secrets_provider`
//!   backend (OS keychain by default, `secrets.yml` fallback when the
//!   profile is set to the file provider).
//! - [`batch`] talks the OpenAI-compatible `POST /audio/transcriptions`
//!   endpoint. It is also compatible with whisper.cpp's `whisper-server`,
//!   AMD Lemonade Server, LocalAI, vLLM and others that implement the same
//!   spec.
//! - [`realtime`] (Phase 2) will talk the OpenAI Realtime transcription
//!   protocol over WebSocket. Same `provider`/`api_url`/`api_key`/`model`
//!   knobs apply.

pub mod batch;
pub mod config;
pub(crate) mod ffi;
pub mod secrets;

pub use config::{ResolvedTranscriptionConfig, TranscriptionProvider, resolve_for_room};

/// Errors that the transcription subsystem can surface to the UI.
///
/// Stringly-typed so it can be funnelled across cxx FFI without bespoke
/// marshalling. The `code` lets the UI choose between localised messages
/// without having to parse `message`.
#[derive(Debug, Clone)]
pub struct TranscriptionError {
    pub code: TranscriptionErrorCode,
    pub message: String,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum TranscriptionErrorCode {
    /// Feature is enabled but `api_url` is empty or `api_key` missing for a
    /// provider that needs one. UI should prompt the user to open settings.
    NotConfigured,
    /// Network-level failure (DNS, TCP, TLS, timeout).
    Network,
    /// Server returned 401/403.
    Unauthorized,
    /// Server returned a non-success HTTP status that isn't auth-related.
    ServerError,
    /// Server response could not be parsed as the expected JSON shape.
    InvalidResponse,
    /// Audio file could not be read or is empty / unreasonably small.
    InvalidAudio,
    /// Internal error (channel closed, runtime gone, etc.).
    Internal,
}

impl TranscriptionError {
    pub fn not_configured(reason: impl Into<String>) -> Self {
        Self {
            code: TranscriptionErrorCode::NotConfigured,
            message: reason.into(),
        }
    }
    pub fn network(reason: impl Into<String>) -> Self {
        Self {
            code: TranscriptionErrorCode::Network,
            message: reason.into(),
        }
    }
    pub fn unauthorized(reason: impl Into<String>) -> Self {
        Self {
            code: TranscriptionErrorCode::Unauthorized,
            message: reason.into(),
        }
    }
    pub fn server_error(reason: impl Into<String>) -> Self {
        Self {
            code: TranscriptionErrorCode::ServerError,
            message: reason.into(),
        }
    }
    pub fn invalid_response(reason: impl Into<String>) -> Self {
        Self {
            code: TranscriptionErrorCode::InvalidResponse,
            message: reason.into(),
        }
    }
    pub fn invalid_audio(reason: impl Into<String>) -> Self {
        Self {
            code: TranscriptionErrorCode::InvalidAudio,
            message: reason.into(),
        }
    }
    pub fn internal(reason: impl Into<String>) -> Self {
        Self {
            code: TranscriptionErrorCode::Internal,
            message: reason.into(),
        }
    }
}

impl std::fmt::Display for TranscriptionError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for TranscriptionError {}
