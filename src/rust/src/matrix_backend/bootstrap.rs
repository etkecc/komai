// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::path::Path;

use matrix_sdk::AuthSession;
use matrix_sdk::store::RoomLoadSettings;
use matrix_sdk::{
    Client, ClientBuildError,
    encryption::{BackupDownloadStrategy, EncryptionSettings},
};
use rand::RngExt;

use crate::ffi;

use super::{
    session_persistence::{
        auth_type_from_auth_session, deserialize_auth_session, load_persisted_session_secrets,
        save_persisted_session_secrets, serialize_auth_session, session_tokens_from_auth_session,
        MatrixPersistedSessionSecrets,
    },
    DerivedMatrixSdkPaths,
};

pub struct MatrixSdkBuildConfig<'a> {
    pub homeserver_url: &'a str,
    pub store_passphrase: Option<&'a str>,
    pub verify_certificates: bool,
}

pub struct MatrixRestorePreview {
    pub has_session: bool,
    pub session_source: String,
    pub auth_type: String,
    pub homeserver_url: String,
    pub user_id: String,
    pub device_id: String,
    pub state_store_root: String,
    pub cache_root: String,
}

pub struct RestoredMatrixBackend {
    pub client: Client,
    pub auth_type: String,
    pub homeserver_url: String,
    pub user_id: String,
    pub device_id: String,
    pub state_store_root: String,
    pub cache_root: String,
}

struct StoredSession {
    homeserver_url: String,
    session: AuthSession,
}

pub async fn build_client(
    config: &MatrixSdkBuildConfig<'_>,
    paths: &DerivedMatrixSdkPaths,
) -> Result<Client, ClientBuildError> {
    let mut builder = Client::builder()
        .homeserver_url(config.homeserver_url)
        .handle_refresh_tokens()
        .with_encryption_settings(EncryptionSettings {
            auto_enable_cross_signing: true,
            auto_enable_backups: true,
            // Lazily fetch individual room keys from server-side key backup when
            // we encounter a UTD via /sync. Without this, recovery() imports
            // cross-signing and the backup decryption key but no room keys are
            // downloaded, so historical encrypted messages stay UTD even after
            // the user pastes a valid recovery key.
            //
            // We deliberately don't use BackupDownloadStrategy::OneShot, which
            // would download the entire backup in a single un-paginated request
            // as soon as recover() runs. matrix-sdk's own comment on that path
            // notes it "doesn't work for any sizeable account" (huge JSON
            // response, decrypt-everything up front).
            // AfterDecryptionFailure is what matrix-sdk-ffi (Element X) uses by
            // default and is the recommended setting for full clients.
            backup_download_strategy: BackupDownloadStrategy::AfterDecryptionFailure,
        })
        .sqlite_store_with_cache_path(
            Path::new(&paths.state_store_root),
            Path::new(&paths.cache_root),
            config.store_passphrase,
        );

    if !config.verify_certificates {
        builder = builder.disable_ssl_verification();
    }

    builder.build().await
}

/// When a client build failed because the on-disk store cipher could not be
/// initialized, returns a clear, user-facing explanation with a hint on how to
/// recover. Returns `None` for every other error so callers keep their generic
/// formatting.
///
/// matrix-sdk's SQLite store is sealed with a random passphrase we persist
/// out-of-band (OS keyring, or a profile-local file when no keyring is
/// available). If that passphrase cannot be read back — most often because the
/// system keyring / Secret Service was locked or unavailable when the session
/// was first stored — the store can no longer be decrypted and the build fails
/// with an `aead` error. We deliberately do NOT delete the store on the user's
/// behalf: the passphrase may merely be temporarily unreadable (a locked
/// keyring), and wiping would destroy a still-recoverable session. Instead we
/// tell the user what happened and how to recover.
pub fn store_cipher_failure_hint(
    error: &ClientBuildError,
    paths: &DerivedMatrixSdkPaths,
) -> Option<String> {
    if !is_store_cipher_init_failure(error) {
        return None;
    }

    Some(format!(
        "The local encrypted store could not be opened because its secret key could not be \
         read. This usually means your system keyring (Secret Service) was locked or \
         unavailable when this session was set up. Unlock your keyring or password manager \
         and try again. As a last resort — this discards locally cached encryption keys and \
         requires signing in again — remove the Matrix store directory: {}",
        paths.matrix_data_root
    ))
}

/// True when the client failed to build because the on-disk store cipher could
/// not be initialized — i.e. the supplied passphrase does not match the one the
/// store was sealed with (or the cipher record is otherwise unreadable). Matched
/// on the rendered error so we do not take a direct dependency on the sqlite
/// store crate just to name one error variant; the wording comes from
/// `matrix_sdk_sqlite::OpenStoreError::InitCipher`.
fn is_store_cipher_init_failure(error: &ClientBuildError) -> bool {
    error.to_string().contains("initialize the store cipher")
}

pub async fn restore_session_preview(profile_id: &str) -> Result<MatrixRestorePreview, String> {
    let Some(restored) = restore_client(profile_id).await? else {
        return Ok(MatrixRestorePreview {
            has_session: false,
            session_source: String::new(),
            auth_type: String::new(),
            homeserver_url: String::new(),
            user_id: String::new(),
            device_id: String::new(),
            state_store_root: String::new(),
            cache_root: String::new(),
        });
    };

    Ok(MatrixRestorePreview {
        has_session: true,
        session_source: "serialized".to_owned(),
        auth_type: restored.auth_type,
        homeserver_url: restored.homeserver_url,
        user_id: restored.user_id,
        device_id: restored.device_id,
        state_store_root: restored.state_store_root,
        cache_root: restored.cache_root,
    })
}

pub async fn restore_client(profile_id: &str) -> Result<Option<RestoredMatrixBackend>, String> {
    tracing::debug!(profile_id, "Attempting to restore persisted matrix-sdk session");

    let persisted_secrets = load_persisted_session_secrets(profile_id);

    let Some(stored_session) = load_stored_session_from(&persisted_secrets)? else {
        tracing::debug!(profile_id, "No serialized matrix-sdk session is stored for this profile");
        return Ok(None);
    };

    let store_passphrase = ensure_store_passphrase_from(profile_id, persisted_secrets);
    let paths = super::derive_matrix_sdk_paths(
        &ffi::matrix_profile_data_root(profile_id),
        &ffi::matrix_profile_cache_root(profile_id),
    );
    let client = build_client(
        &MatrixSdkBuildConfig {
            homeserver_url: &stored_session.homeserver_url,
            store_passphrase: Some(&store_passphrase),
            verify_certificates: true,
        },
        &paths,
    )
    .await
    .map_err(|e| {
        store_cipher_failure_hint(&e, &paths)
            .unwrap_or_else(|| format!("failed to build matrix-sdk client for restore: {e}"))
    })?;

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

    tracing::info!(
        profile_id,
        homeserver_url = %stored_session.homeserver_url,
        user_id = %stored_session.session.meta().user_id,
        device_id = %stored_session.session.meta().device_id,
        "Restored persisted matrix-sdk session"
    );

    Ok(Some(RestoredMatrixBackend {
        client,
        auth_type: auth_type_from_auth_session(&stored_session.session).to_owned(),
        homeserver_url: stored_session.homeserver_url,
        user_id: stored_session.session.meta().user_id.to_string(),
        device_id: stored_session.session.meta().device_id.to_string(),
        state_store_root: paths.state_store_root,
        cache_root: paths.cache_root,
    }))
}

fn load_stored_session_from(
    persisted_secrets: &MatrixPersistedSessionSecrets,
) -> Result<Option<StoredSession>, String> {
    if persisted_secrets.serialized_session.trim().is_empty()
        || persisted_secrets.homeserver_url.trim().is_empty()
    {
        return Ok(None);
    }

    let session = deserialize_auth_session(&persisted_secrets.serialized_session)?;
    Ok(Some(StoredSession {
        homeserver_url: persisted_secrets.homeserver_url.clone(),
        session,
    }))
}

pub(crate) fn ensure_store_passphrase(profile_id: &str) -> String {
    let persisted = load_persisted_session_secrets(profile_id);
    ensure_store_passphrase_from(profile_id, persisted)
}

fn ensure_store_passphrase_from(
    profile_id: &str,
    persisted: MatrixPersistedSessionSecrets,
) -> String {
    if !persisted.store_passphrase.trim().is_empty() {
        return persisted.store_passphrase;
    }

    let mut rng = rand::rng();
    let store_passphrase: String = (&mut rng)
        .sample_iter(rand::distr::Alphanumeric)
        .take(32)
        .map(char::from)
        .collect();

    if !save_persisted_session_secrets(
        profile_id,
        &MatrixPersistedSessionSecrets {
            store_passphrase: store_passphrase.clone(),
            homeserver_url: persisted.homeserver_url,
            serialized_session: persisted.serialized_session,
        },
    ) {
        tracing::error!(
            profile_id,
            "Failed to persist the freshly generated matrix-sdk store passphrase; the local \
             store created with it will not be decryptable on the next launch"
        );
    }

    store_passphrase
}

pub(crate) fn persist_current_session(
    profile_id: &str,
    store_passphrase: &str,
    homeserver_url: &str,
    client: &Client,
) -> Result<(), String> {
    let session =
        client.session().ok_or_else(|| "matrix-sdk client has no authenticated session to persist".to_owned())?;
    let serialized_session = serialize_auth_session(&session)?;

    if !save_persisted_session_secrets(
        profile_id,
        &MatrixPersistedSessionSecrets {
            store_passphrase: store_passphrase.to_owned(),
            homeserver_url: homeserver_url.to_owned(),
            serialized_session,
        },
    ) {
        // Losing this write after an OAuth token refresh strands a rotated
        // refresh token: the persisted session keeps the pre-rotation token
        // and the server rejects it with invalid_grant on the next launch.
        tracing::error!(
            profile_id,
            "Failed to persist the matrix-sdk auth session to the secure store"
        );
        return Err("failed to persist the matrix-sdk auth session to the secure store".to_owned());
    }

    Ok(())
}

pub(crate) fn configure_session_callbacks(
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
                        "no serialized auth session available for reload",
                    )));
                }

                let session = deserialize_auth_session(&persisted.serialized_session)
                    .map_err(std::io::Error::other)?;
                Ok(session_tokens_from_auth_session(&session))
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
