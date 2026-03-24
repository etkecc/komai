// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::path::Path;

use matrix_sdk::authentication::matrix::MatrixSession;
use matrix_sdk::store::RoomLoadSettings;
use matrix_sdk::{Client, ClientBuildError};
use rand::RngExt;

use crate::ffi;

use super::{
    session_persistence::{
        load_persisted_session_secrets, save_persisted_session_secrets,
        PersistedMatrixSessionSecrets,
    },
    DerivedMatrixSdkPaths,
};

pub struct MatrixSdkBuildConfig<'a> {
    pub homeserver_url: &'a str,
    pub store_passphrase: Option<&'a str>,
}

pub struct MatrixRestorePreview {
    pub has_session: bool,
    pub session_source: String,
    pub homeserver_url: String,
    pub user_id: String,
    pub device_id: String,
    pub state_store_root: String,
    pub cache_root: String,
}

struct StoredSession {
    homeserver_url: String,
    session: MatrixSession,
}

pub async fn build_client(
    config: &MatrixSdkBuildConfig<'_>,
    paths: &DerivedMatrixSdkPaths,
) -> Result<Client, ClientBuildError> {
    Client::builder()
        .homeserver_url(config.homeserver_url)
        .sqlite_store_with_cache_path(
            Path::new(&paths.state_store_root),
            Path::new(&paths.cache_root),
            config.store_passphrase,
        )
        .build()
        .await
}

pub async fn restore_session_preview(profile_id: &str) -> Result<MatrixRestorePreview, String> {
    let Some(stored_session) = load_stored_session(profile_id)? else {
        return Ok(MatrixRestorePreview {
            has_session: false,
            session_source: String::new(),
            homeserver_url: String::new(),
            user_id: String::new(),
            device_id: String::new(),
            state_store_root: String::new(),
            cache_root: String::new(),
        });
    };

    let store_passphrase = ensure_store_passphrase(profile_id);
    let paths = super::derive_matrix_sdk_paths(
        &ffi::matrix_profile_data_root(profile_id),
        &ffi::matrix_profile_cache_root(profile_id),
    );
    let client = build_client(
        &MatrixSdkBuildConfig {
            homeserver_url: &stored_session.homeserver_url,
            store_passphrase: Some(&store_passphrase),
        },
        &paths,
    )
    .await
    .map_err(|e| format!("failed to build matrix-sdk client for restore: {e}"))?;

    configure_session_callbacks(
        &client,
        profile_id,
        &store_passphrase,
        &stored_session.homeserver_url,
    )?;

    client
        .restore_session_with(stored_session.session.clone(), RoomLoadSettings::default())
        .await
        .map_err(|e| format!("failed to restore matrix-sdk session: {e}"))?;

    persist_current_session(
        profile_id,
        &store_passphrase,
        &stored_session.homeserver_url,
        &client,
    )?;

    Ok(MatrixRestorePreview {
        has_session: true,
        session_source: "serialized".to_owned(),
        homeserver_url: stored_session.homeserver_url,
        user_id: stored_session.session.meta.user_id.to_string(),
        device_id: stored_session.session.meta.device_id.to_string(),
        state_store_root: paths.state_store_root,
        cache_root: paths.cache_root,
    })
}

fn load_stored_session(profile_id: &str) -> Result<Option<StoredSession>, String> {
    let persisted_secrets = load_persisted_session_secrets(profile_id);

    if persisted_secrets.serialized_session.trim().is_empty()
        || persisted_secrets.homeserver_url.trim().is_empty()
    {
        return Ok(None);
    }

    let session = deserialize_matrix_session(&persisted_secrets.serialized_session)?;
    Ok(Some(StoredSession {
        homeserver_url: persisted_secrets.homeserver_url,
        session,
    }))
}

fn ensure_store_passphrase(profile_id: &str) -> String {
    let persisted = load_persisted_session_secrets(profile_id);
    if !persisted.store_passphrase.trim().is_empty() {
        return persisted.store_passphrase;
    }

    let mut rng = rand::rng();
    let store_passphrase: String = (&mut rng)
        .sample_iter(rand::distr::Alphanumeric)
        .take(32)
        .map(char::from)
        .collect();

    save_persisted_session_secrets(
        profile_id,
        &PersistedMatrixSessionSecrets {
            store_passphrase: store_passphrase.clone(),
            homeserver_url: persisted.homeserver_url,
            serialized_session: persisted.serialized_session,
        },
    );

    store_passphrase
}

fn deserialize_matrix_session(serialized_session: &str) -> Result<MatrixSession, String> {
    serde_json::from_str(serialized_session)
        .map_err(|e| format!("failed to deserialize persisted MatrixSession: {e}"))
}

fn serialize_matrix_session(session: &MatrixSession) -> Result<String, String> {
    serde_json::to_string(session).map_err(|e| format!("failed to serialize MatrixSession: {e}"))
}

fn persist_current_session(
    profile_id: &str,
    store_passphrase: &str,
    homeserver_url: &str,
    client: &Client,
) -> Result<(), String> {
    let session = client
        .matrix_auth()
        .session()
        .ok_or_else(|| "matrix-sdk client has no matrix-auth session to persist".to_owned())?;
    let serialized_session = serialize_matrix_session(&session)?;

    save_persisted_session_secrets(
        profile_id,
        &PersistedMatrixSessionSecrets {
            store_passphrase: store_passphrase.to_owned(),
            homeserver_url: homeserver_url.to_owned(),
            serialized_session,
        },
    );

    Ok(())
}

fn configure_session_callbacks(
    client: &Client,
    profile_id: &str,
    store_passphrase: &str,
    homeserver_url: &str,
) -> Result<(), String> {
    let reload_profile_id = profile_id.to_owned();
    let save_profile_id = profile_id.to_owned();
    let save_store_passphrase = store_passphrase.to_owned();
    let save_homeserver_url = homeserver_url.to_owned();

    client
        .set_session_callbacks(
            Box::new(move |_| {
                let persisted = load_persisted_session_secrets(&reload_profile_id);
                if persisted.serialized_session.trim().is_empty() {
                    return Err(Box::new(std::io::Error::other(
                        "no serialized matrix session available for reload",
                    )));
                }

                let session = deserialize_matrix_session(&persisted.serialized_session)
                    .map_err(std::io::Error::other)?;
                Ok(session.tokens)
            }),
            Box::new(move |client| {
                persist_current_session(
                    &save_profile_id,
                    &save_store_passphrase,
                    &save_homeserver_url,
                    &client,
                )
                .map_err(std::io::Error::other)?;
                Ok(())
            }),
        )
        .map_err(|e| format!("failed to register matrix-sdk session callbacks: {e}"))
}
