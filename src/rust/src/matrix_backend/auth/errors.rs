// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Pretty-printers for the various matrix-sdk error types, used to
//! surface meaningful failure messages back to the C++ side instead of
//! the raw Debug output.

use super::*;


/// Translated to user-visible text in C++ StateEventText::translateAuthError().
/// When adding or changing constant error strings here, update that function too.
pub(super) fn format_client_build_error(error: &ClientBuildError) -> String {
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

pub(super) fn format_sdk_error(prefix: &str, error: &MatrixSdkError) -> String {
    if let Some(error) = error.as_client_api_error() {
        return format_client_api_error(prefix, error);
    }

    match error {
        MatrixSdkError::Http(error) => format_http_error(prefix, error),
        _ => format!("{prefix}: {error}"),
    }
}

pub(super) fn format_http_error(prefix: &str, error: &HttpError) -> String {
    if let Some(error) = error.as_client_api_error() {
        return format_client_api_error(prefix, error);
    }

    if let HttpError::Reqwest(error) = error {
        return format!("{prefix}: {error}");
    }

    format!("{prefix}: {error}")
}

pub(super) fn format_ruma_api_error(prefix: &str, error: &RumaApiError) -> String {
    if let RumaApiError::MatrixError(error) = error {
        return format_client_api_error(prefix, error);
    }

    format!("{prefix}: {error}")
}

pub(super) fn format_client_api_error(prefix: &str, error: &matrix_sdk::ruma::api::error::Error) -> String {
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
