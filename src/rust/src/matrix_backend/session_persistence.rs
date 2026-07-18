// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::{
    AuthSession,
    SessionTokens,
    authentication::{
        matrix::MatrixSession,
        oauth::{ClientId, OAuthSession, UserSession},
    },
};
use serde::{Deserialize, Serialize};

use crate::ffi;

#[derive(Serialize, Deserialize)]
struct PersistedAuthSessionEnvelope {
    auth_type: String,
    session_json: String,
}

#[derive(Serialize, Deserialize)]
struct PersistedOAuthSession {
    client_id: String,
    user: UserSession,
}

pub(crate) use crate::ffi::MatrixPersistedSessionSecrets;

pub fn load_persisted_session_secrets(profile_id: &str) -> MatrixPersistedSessionSecrets {
    ffi::matrix_load_session_secrets(profile_id)
}

#[must_use]
pub fn save_persisted_session_secrets(
    profile_id: &str,
    secrets: &MatrixPersistedSessionSecrets,
) -> bool {
    ffi::matrix_save_session_secrets(
        profile_id,
        &secrets.store_passphrase,
        &secrets.homeserver_url,
        &secrets.serialized_session,
    )
}

pub fn clear_persisted_session_secrets(profile_id: &str) {
    ffi::matrix_clear_session_secrets(profile_id);
}

pub fn serialize_auth_session(session: &AuthSession) -> Result<String, String> {
    let auth_type = auth_type_from_auth_session(session).to_owned();
    let envelope = match session {
        AuthSession::Matrix(session) => PersistedAuthSessionEnvelope {
            auth_type,
            session_json: serde_json::to_string(session)
                .map_err(|e| format!("failed to serialize MatrixSession: {e}"))?,
        },
        AuthSession::OAuth(session) => PersistedAuthSessionEnvelope {
            auth_type,
            session_json: serde_json::to_string(&PersistedOAuthSession {
                client_id: session.client_id.as_str().to_owned(),
                user: session.user.clone(),
            })
            .map_err(|e| format!("failed to serialize OAuthSession: {e}"))?,
        },
        _ => {
            return Err("unsupported authenticated session type for persistence".to_owned());
        }
    };

    serde_json::to_string(&envelope)
        .map_err(|e| format!("failed to serialize persisted auth session envelope: {e}"))
}

pub fn deserialize_auth_session(serialized_session: &str) -> Result<AuthSession, String> {
    if let Ok(envelope) = serde_json::from_str::<PersistedAuthSessionEnvelope>(serialized_session) {
        return match envelope.auth_type.as_str() {
            "matrix" => {
                let session = serde_json::from_str::<MatrixSession>(&envelope.session_json)
                    .map_err(|e| format!("failed to deserialize MatrixSession: {e}"))?;
                Ok(AuthSession::Matrix(session))
            }
            "oauth" => {
                let session = serde_json::from_str::<PersistedOAuthSession>(&envelope.session_json)
                    .map_err(|e| format!("failed to deserialize OAuthSession: {e}"))?;
                Ok(AuthSession::OAuth(Box::new(OAuthSession {
                    client_id: ClientId::new(session.client_id),
                    user: session.user,
                })))
            }
            other => Err(format!("unsupported persisted auth session type '{other}'")),
        };
    }

    // Backward compatibility with the old migration-branch format where we
    // stored a raw MatrixSession JSON blob directly.
    let session = serde_json::from_str::<MatrixSession>(serialized_session)
        .map_err(|e| format!("failed to deserialize persisted auth session: {e}"))?;
    Ok(AuthSession::Matrix(session))
}

pub fn session_tokens_from_auth_session(session: &AuthSession) -> SessionTokens {
    match session {
        AuthSession::Matrix(session) => session.tokens.clone(),
        AuthSession::OAuth(session) => session.user.tokens.clone(),
        _ => SessionTokens {
            access_token: String::new(),
            refresh_token: None,
        },
    }
}

pub fn auth_type_from_auth_session(session: &AuthSession) -> &'static str {
    match session {
        AuthSession::Matrix(_) => "matrix",
        AuthSession::OAuth(_) => "oauth",
        _ => "unknown",
    }
}
