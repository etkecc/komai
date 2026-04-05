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
    let encryption = client.encryption();

    let own_device_is_verified = {
        let cross_signed = encryption
            .get_own_device()
            .await
            .map_err(|e| format!("failed to inspect current device verification state: {e}"))?
            .is_some_and(|device| device.is_cross_signed_by_owner());

        if cross_signed {
            true
        } else {
            // The device may not appear as cross-signed if the server's /keys/query
            // response doesn't reflect the signature (e.g. race condition or server
            // quirk). Fall back to checking whether we hold all cross-signing keys
            // and recovery is enabled — if so, the device has full signing authority.
            let cross_signing_complete = encryption
                .cross_signing_status()
                .await
                .is_some_and(|s| s.is_complete());
            let recovery_enabled =
                encryption.recovery().state() == RecoveryState::Enabled;
            cross_signing_complete && recovery_enabled
        }
    };

    let has_unverified_own_devices = if let Some(user_id) = client.user_id() {
        let current_device_id = client.device_id().map(|device_id| device_id.to_owned());

        // Use the server's /devices endpoint as the authoritative list of active
        // sessions, since the local crypto store can retain stale devices that
        // have already been signed out.
        let active_device_ids: std::collections::HashSet<_> = client
            .devices()
            .await
            .map(|response| {
                response
                    .devices
                    .into_iter()
                    .map(|d| d.device_id)
                    .collect()
            })
            .unwrap_or_default();

        let devices = encryption
            .get_user_devices(user_id)
            .await
            .map_err(|e| format!("failed to inspect own device list: {e}"))?;

        devices.devices().any(|device| {
            let is_current_device = current_device_id
                .as_ref()
                .is_some_and(|current_device_id| device.device_id() == current_device_id);

            !is_current_device
                && !device.is_dehydrated()
                && device.curve25519_key().is_some()
                && !device.is_cross_signed_by_owner()
                && active_device_ids.contains(device.device_id())
        })
    } else {
        false
    };

    Ok(MatrixRecoveryStatus {
        state: recovery_state_name(encryption.recovery().state()),
        has_devices_to_verify_against: encryption
            .has_devices_to_verify_against()
            .await
            .map_err(|e| format!("failed to inspect available verification devices: {e}"))?,
        own_device_is_verified,
        has_unverified_own_devices,
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

pub async fn setup_recovery(
    handle_id: u64,
    use_ssss: bool,
    passphrase: &str,
    encryption_backup_online_enabled: bool,
) -> Result<MatrixSetupRecoveryResult, String> {
    let client = client_for_handle(handle_id)?;
    client.encryption().wait_for_e2ee_initialization_tasks().await;

    let trimmed_passphrase = passphrase.trim();
    let encryption = client.encryption();

    if let Some(status) = encryption.cross_signing_status().await
        && !status.is_complete()
    {
        encryption
            .bootstrap_cross_signing_if_needed(None)
            .await
            .map_err(|e| format!("failed to bootstrap cross-signing identity: {e}"))?;
    }

    let recovery_key = if use_ssss {
        if encryption_backup_online_enabled {
            let recovery = encryption.recovery();

            // Delete any stale backup left on the server (e.g. after an identity reset
            // that left the server in a half-cleaned state). recovery.enable() refuses
            // to proceed if a backup already exists.
            if recovery.state() != RecoveryState::Enabled
                && encryption
                    .backups()
                    .fetch_exists_on_server()
                    .await
                    .unwrap_or(false)
            {
                encryption
                    .backups()
                    .disable_and_delete()
                    .await
                    .map_err(|e| format!("failed to remove stale server backup: {e}"))?;
            }

            let enable = if trimmed_passphrase.is_empty() {
                recovery.enable()
            } else {
                recovery.enable().with_passphrase(trimmed_passphrase)
            };

            let key = enable
                .await
                .map_err(|e| format!("failed to enable encrypted recovery: {e}"))?;

            // Immediately recover with the new key so the local device imports
            // all secrets and is marked as verified (cross-signed).
            recovery
                .recover(&key)
                .await
                .map_err(|e| format!("failed to recover secrets after enabling recovery: {e}"))?;

            key
        } else {
            let secret_storage = encryption.secret_storage();
            let create_store = if trimmed_passphrase.is_empty() {
                secret_storage.create_secret_store()
            } else {
                secret_storage.create_secret_store().with_passphrase(trimmed_passphrase)
            };

            create_store
                .await
                .map_err(|e| format!("failed to create secure secret storage: {e}"))?
                .secret_storage_key()
        }
    } else {
        if encryption_backup_online_enabled && !encryption.backups().are_enabled().await {
            encryption
                .backups()
                .create()
                .await
                .map_err(|e| format!("failed to create encryption key backup: {e}"))?;
        }

        String::new()
    };

    Ok(MatrixSetupRecoveryResult { recovery_key })
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
