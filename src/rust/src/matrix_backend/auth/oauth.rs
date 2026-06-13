// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! OAuth (Authorization Code + PKCE) login flow against an MSC2965-style
//! delegated identity provider. Keeps the in-flight state map keyed by a
//! login id the C++ side polls.

use super::*;
use super::login::persist_login;
use super::super::derive_matrix_sdk_paths;

pub(super) struct PendingOAuthLogin {
    profile_id: String,
    client: Client,
    initial_device_display_name: String,
}

pub(super) static NEXT_OAUTH_LOGIN_ID: AtomicU64 = AtomicU64::new(1);

pub(super) fn pending_oauth_logins() -> &'static Mutex<HashMap<u64, PendingOAuthLogin>> {
    static PENDING: OnceLock<Mutex<HashMap<u64, PendingOAuthLogin>>> = OnceLock::new();
    PENDING.get_or_init(|| Mutex::new(HashMap::new()))
}

pub(super) fn oauth_client_registration_data(redirect_uri: &Url) -> Result<ClientRegistrationData, String> {
    let client_uri =
        Url::parse("https://github.com/etkecc/komai").map_err(|e| format!("invalid OAuth client URI: {e}"))?;

    let mut metadata = ClientMetadata::new(
        ApplicationType::Native,
        vec![OAuthGrantType::AuthorizationCode { redirect_uris: vec![redirect_uri.clone()] }],
        Localized::new(client_uri, None),
    );
    metadata.client_name = Some(Localized::new("Komai".to_owned(), None));

    let raw =
        Raw::new(&metadata).map_err(|e| format!("failed to serialize OAuth client metadata: {e}"))?;
    Ok(ClientRegistrationData::new(raw))
}


pub async fn start_oauth_login(
    profile_id: &str,
    homeserver_url: &str,
    redirect_url: &str,
    user_id_hint: &str,
    device_id: &str,
    initial_device_display_name: &str,
    verify_certificates: bool,
) -> Result<MatrixOauthLoginStartResult, String> {
    tracing::info!(
        profile_id,
        homeserver_url,
        reusing_device_id = !device_id.trim().is_empty(),
        verify_certificates,
        "Starting OAuth login"
    );

    let client = build_oauth_login_client(profile_id, homeserver_url, verify_certificates).await?;
    let redirect_uri =
        Url::parse(redirect_url).map_err(|e| format!("invalid OAuth redirect URL: {e}"))?;
    let registration_data = oauth_client_registration_data(&redirect_uri)?;
    let device_id = (!device_id.trim().is_empty()).then(|| device_id.trim().into());

    let mut builder = client
        .oauth()
        .login(redirect_uri, device_id, Some(registration_data), None);
    if !user_id_hint.trim().is_empty() {
        builder = builder.login_hint(user_id_hint.trim().to_owned());
    }

    let authorization = builder
        .build()
        .await
        .map_err(|e| format!("failed to start OAuth login: {e}"))?;

    let login_id = NEXT_OAUTH_LOGIN_ID.fetch_add(1, Ordering::Relaxed);
    pending_oauth_logins()
        .lock()
        .expect("poisoned OAuth login registry mutex")
        .insert(
            login_id,
            PendingOAuthLogin {
                profile_id: profile_id.to_owned(),
                client,
                initial_device_display_name: initial_device_display_name.to_owned(),
            },
        );

    Ok(MatrixOauthLoginStartResult {
        login_id,
        login_url: authorization.url.to_string(),
    })
}

pub async fn finish_oauth_login(
    login_id: u64,
    callback_query: &str,
) -> Result<MatrixLoginResult, String> {
    let pending = pending_oauth_logins()
        .lock()
        .expect("poisoned OAuth login registry mutex")
        .remove(&login_id)
        .ok_or_else(|| format!("unknown pending OAuth login: {login_id}"))?;

    let callback_query = callback_query.trim().trim_start_matches('?').to_owned();
    if callback_query.is_empty() {
        return Err("OAuth callback query cannot be empty".to_owned());
    }

    pending
        .client
        .oauth()
        .finish_login(UrlOrQuery::Query(callback_query))
        .await
        .map_err(|e| format!("failed to finish OAuth login: {e}"))?;

    if !pending.initial_device_display_name.trim().is_empty()
        && let Some(current_device_id) = pending.client.device_id()
        && let Err(error) = pending
            .client
            .rename_device(current_device_id, pending.initial_device_display_name.trim())
            .await
    {
        tracing::warn!(
            login_id,
            error = %error,
            "Failed to set OAuth device display name after login"
        );
    }

    pending.client.encryption().wait_for_e2ee_initialization_tasks().await;

    // WARNING: Do NOT call client.sync_once() or any v2 /sync API here.
    //
    // The v2 sync stores its string-format next_batch token (e.g. "s3726933_...")
    // into the crypto store's next_batch_token key. The EncryptionSyncService
    // (inside SyncService) later reads this back and sends it as sliding sync's
    // to_device.since field, which the server expects to be an integer. This
    // causes M_INVALID_PARAM errors that permanently break encryption sync
    // (verification, key sharing, all to-device e2ee) until the profile's
    // crypto store is deleted and recreated.
    //
    // All syncing is handled by SyncService via sliding sync. Any initialization
    // that sync_once was previously used for is covered by the SyncService's
    // initial sliding sync iteration.

    let user_id = pending
        .client
        .user_id()
        .ok_or_else(|| "matrix-sdk OAuth login finished without a user id".to_owned())?
        .to_string();
    let device_id = pending
        .client
        .device_id()
        .ok_or_else(|| "matrix-sdk OAuth login finished without a device id".to_owned())?
        .to_string();
    let access_token = pending
        .client
        .session_tokens()
        .ok_or_else(|| "matrix-sdk OAuth login finished without session tokens".to_owned())?
        .access_token;

    persist_login(
        &pending.profile_id,
        &pending.client,
        user_id,
        access_token,
        device_id,
    )
}

pub fn cancel_oauth_login(login_id: u64) -> Result<(), String> {
    // Enter the Tokio runtime context so that the Client (and its SqliteStateStore /
    // deadpool connection pool) can be dropped safely.  This function is sync and may
    // be called from a Qt thread that has no reactor — without the guard, the drop
    // chain panics with "there is no reactor running".
    let _rt_guard = crate::ffi::runtime().enter();

    pending_oauth_logins()
        .lock()
        .expect("poisoned OAuth login registry mutex")
        .remove(&login_id)
        .map(|_| ())
        .ok_or_else(|| format!("unknown pending OAuth login: {login_id}"))
}

pub(super) async fn build_oauth_login_client(
    profile_id: &str,
    homeserver_url: &str,
    verify_certificates: bool,
) -> Result<Client, String> {
    let store_passphrase = bootstrap::ensure_store_passphrase(profile_id);
    let paths = derive_matrix_sdk_paths(
        &crate::ffi::matrix_profile_data_root(profile_id),
        &crate::ffi::matrix_profile_cache_root(profile_id),
    );

    bootstrap::build_client(
        &bootstrap::MatrixSdkBuildConfig {
            homeserver_url,
            store_passphrase: Some(&store_passphrase),
            verify_certificates,
        },
        &paths,
    )
    .await
    .map_err(|error| {
        bootstrap::store_cipher_failure_hint(&error, &paths)
            .unwrap_or_else(|| format!("failed to build matrix-sdk OAuth client: {error}"))
    })
}
