// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::{
    encryption::{CrossSigningResetAuthType, recovery::RecoveryState},
    ruma::api::client::uiaa::{self, AuthData},
};

use super::*;

fn recovery_state_name(state: RecoveryState) -> String {
    match state {
        RecoveryState::Unknown => "unknown",
        RecoveryState::Enabled => "enabled",
        RecoveryState::Disabled => "disabled",
        RecoveryState::Incomplete => "incomplete",
    }
    .to_owned()
}

pub async fn fetch_recovery_status(handle_id: u64) -> Result<MatrixRecoveryStatus, String> {
    let client = client_for_handle(handle_id)?;
    client.encryption().wait_for_e2ee_initialization_tasks().await;

    Ok(MatrixRecoveryStatus {
        state: recovery_state_name(client.encryption().recovery().state()),
    })
}

pub async fn recover_encryption_secrets(
    handle_id: u64,
    key_or_passphrase: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    client.encryption().wait_for_e2ee_initialization_tasks().await;

    client
        .encryption()
        .recovery()
        .recover(key_or_passphrase.trim())
        .await
        .map_err(|e| format!("failed to recover encryption secrets: {e}"))
}

pub async fn start_reset_encryption_identity(
    handle_id: u64,
) -> Result<MatrixResetEncryptionIdentityResult, String> {
    let client = client_for_handle(handle_id)?;
    client.encryption().wait_for_e2ee_initialization_tasks().await;

    let reset = client
        .encryption()
        .recovery()
        .reset_identity()
        .await
        .map_err(|e| format!("failed to reset encryption identity: {e}"))?;

    let pending = pending_identity_reset_for_handle(handle_id)?;
    let mut pending = pending
        .lock()
        .expect("poisoned matrix backend pending identity reset mutex");
    *pending = None;

    let Some(handle) = reset else {
        return Ok(MatrixResetEncryptionIdentityResult {
            completed: true,
            auth_type: String::new(),
            approval_url: String::new(),
        });
    };

    let result = match handle.auth_type() {
        CrossSigningResetAuthType::Uiaa(_) => MatrixResetEncryptionIdentityResult {
            completed: false,
            auth_type: "password".to_owned(),
            approval_url: String::new(),
        },
        CrossSigningResetAuthType::OAuth(info) => MatrixResetEncryptionIdentityResult {
            completed: false,
            auth_type: "oauth".to_owned(),
            approval_url: info.approval_url.to_string(),
        },
    };

    *pending = Some(handle);
    Ok(result)
}

pub async fn continue_reset_encryption_identity_with_password(
    handle_id: u64,
    password: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let pending = pending_identity_reset_for_handle(handle_id)?;
    let handle = {
        let mut pending = pending
            .lock()
            .expect("poisoned matrix backend pending identity reset mutex");
        pending
            .take()
            .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} has no pending encryption identity reset"))?
    };

    let CrossSigningResetAuthType::Uiaa(uiaa_info) = handle.auth_type().clone() else {
        let mut pending = pending
            .lock()
            .expect("poisoned matrix backend pending identity reset mutex");
        *pending = Some(handle);
        return Err("pending encryption identity reset does not accept password authentication"
                     .to_owned());
    };

    let user_id = client
        .user_id()
        .ok_or_else(|| "matrix-sdk client has no authenticated user id".to_owned())?;
    let mut password_auth = uiaa::Password::new(user_id.to_owned().into(), password.to_owned());
    password_auth.session = uiaa_info.session.clone();

    match handle.reset(Some(AuthData::Password(password_auth))).await {
        Ok(()) => Ok(()),
        Err(e) => {
            let mut pending = pending
                .lock()
                .expect("poisoned matrix backend pending identity reset mutex");
            *pending = Some(handle);
            Err(format!("failed to complete encryption identity reset: {e}"))
        }
    }
}

pub async fn continue_reset_encryption_identity_after_approval(
    handle_id: u64,
) -> Result<(), String> {
    let pending = pending_identity_reset_for_handle(handle_id)?;
    let handle = {
        let mut pending = pending
            .lock()
            .expect("poisoned matrix backend pending identity reset mutex");
        pending
            .take()
            .ok_or_else(|| format!("matrix-sdk backend runtime handle {handle_id} has no pending encryption identity reset"))?
    };

    let CrossSigningResetAuthType::OAuth(_) = handle.auth_type() else {
        let mut pending = pending
            .lock()
            .expect("poisoned matrix backend pending identity reset mutex");
        *pending = Some(handle);
        return Err("pending encryption identity reset does not use approval-link authentication"
                     .to_owned());
    };

    match handle.reset(None).await {
        Ok(()) => Ok(()),
        Err(e) => {
            let mut pending = pending
                .lock()
                .expect("poisoned matrix backend pending identity reset mutex");
            *pending = Some(handle);
            Err(format!("failed to complete encryption identity reset: {e}"))
        }
    }
}

pub async fn cancel_reset_encryption_identity(handle_id: u64) -> Result<(), String> {
    let pending = pending_identity_reset_for_handle(handle_id)?;
    let handle = {
        let mut pending = pending
            .lock()
            .expect("poisoned matrix backend pending identity reset mutex");
        pending.take()
    };

    if let Some(handle) = handle {
        handle.cancel().await;
    }

    Ok(())
}
