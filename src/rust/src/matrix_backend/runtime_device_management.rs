// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use matrix_sdk::{
    AuthSession,
    ruma::{
        OwnedDeviceId,
        api::client::{
            discovery::get_authorization_server_metadata::v1::{
                AccountManagementAction, AccountManagementActionData, DeviceDeleteData,
            },
            uiaa::{self, AuthData},
        },
    },
};

use super::*;

fn supports_password_auth(uiaa_info: &uiaa::UiaaInfo) -> bool {
    uiaa_info
        .flows
        .iter()
        .any(|flow| flow.stages.iter().any(|stage| *stage == uiaa::AuthType::Password))
}

pub async fn start_sign_out_device(
    handle_id: u64,
    device_id: &str,
) -> Result<MatrixDeviceSignOutResult, String> {
    let client = client_for_handle(handle_id)?;
    if device_id.trim().is_empty() {
        return Err("device id cannot be empty".to_owned());
    }

    let parsed_device_id: OwnedDeviceId = device_id.trim().into();
    let pending = pending_device_sign_out_for_handle(handle_id)?;
    {
        let mut pending = pending
            .lock()
            .expect("poisoned matrix backend pending device sign-out mutex");
        *pending = None;
    }

    match client
        .session()
        .ok_or_else(|| "matrix-sdk client has no authenticated session".to_owned())?
    {
        AuthSession::OAuth(_) => {
            let oauth = client.oauth();
            let metadata = oauth
                .server_metadata()
                .await
                .map_err(|e| format!("failed to fetch OAuth server metadata: {e}"))?;
            if metadata.account_management_uri.is_none() {
                return Err(
                    "This homeserver does not advertise an OAuth account-management URL for session management."
                        .to_owned(),
                );
            }

            let supports_device_delete = metadata
                .account_management_actions_supported
                .contains(&AccountManagementAction::DeviceDelete)
                || metadata
                    .account_management_actions_supported
                    .contains(&AccountManagementAction::UnstableSessionEnd);
            let supports_devices_list = metadata
                .account_management_actions_supported
                .contains(&AccountManagementAction::DevicesList)
                || metadata
                    .account_management_actions_supported
                    .contains(&AccountManagementAction::UnstableSessionsList);

            let action = if supports_device_delete {
                Some(AccountManagementActionData::DeviceDelete(DeviceDeleteData::new(
                    &parsed_device_id,
                )))
            } else if supports_devices_list {
                Some(AccountManagementActionData::DevicesList)
            } else {
                None
            };

            let approval_url = if let Some(action) = action {
                metadata
                    .account_management_url_with_action(action)
                    .map(|url| url.to_string())
                    .unwrap_or_else(|| {
                        metadata
                            .account_management_uri
                            .as_ref()
                            .map(|url| url.to_string())
                            .unwrap_or_default()
                    })
            } else {
                metadata
                    .account_management_uri
                    .as_ref()
                    .map(|url| url.to_string())
                    .unwrap_or_default()
            };

            Ok(MatrixDeviceSignOutResult {
                completed: false,
                auth_type: "oauth".to_owned(),
                approval_url,
            })
        }
        AuthSession::Matrix(_) => match client.delete_devices(&[parsed_device_id.clone()], None).await {
            Ok(_) => Ok(MatrixDeviceSignOutResult {
                completed: true,
                auth_type: String::new(),
                approval_url: String::new(),
            }),
            Err(error) => {
                if let Some(uiaa_info) = error.as_uiaa_response() {
                    if !supports_password_auth(uiaa_info) {
                        return Err(
                            "Signing out this device requires unsupported interactive authentication stages."
                                .to_owned(),
                        );
                    }

                    let mut pending = pending
                        .lock()
                        .expect("poisoned matrix backend pending device sign-out mutex");
                    *pending = Some(PendingDeviceSignOut {
                        device_id: parsed_device_id,
                        uiaa_info: uiaa_info.clone(),
                    });

                    return Ok(MatrixDeviceSignOutResult {
                        completed: false,
                        auth_type: "password".to_owned(),
                        approval_url: String::new(),
                    });
                }

                Err(format!("failed to sign out device '{device_id}': {error}"))
            }
        },
        _ => Err("unsupported matrix-sdk authenticated session type for device sign-out"
            .to_owned()),
    }
}

pub async fn rename_device(
    handle_id: u64,
    device_id: &str,
    display_name: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    if device_id.trim().is_empty() {
        return Err("device id cannot be empty".to_owned());
    }

    let trimmed_display_name = display_name.trim();
    if trimmed_display_name.is_empty() {
        return Err("device display name cannot be empty".to_owned());
    }

    let parsed_device_id: OwnedDeviceId = device_id.trim().into();
    client
        .rename_device(&parsed_device_id, trimmed_display_name)
        .await
        .map_err(|error| format!("failed to rename device '{device_id}': {error}"))?;

    Ok(())
}

pub async fn continue_sign_out_device_with_password(
    handle_id: u64,
    password: &str,
) -> Result<(), String> {
    let client = client_for_handle(handle_id)?;
    let pending = pending_device_sign_out_for_handle(handle_id)?;
    let pending_sign_out = {
        let mut pending = pending
            .lock()
            .expect("poisoned matrix backend pending device sign-out mutex");
        pending.take().ok_or_else(|| {
            format!(
                "matrix-sdk backend runtime handle {handle_id} has no pending device sign-out"
            )
        })?
    };

    let user_id = client
        .user_id()
        .ok_or_else(|| "matrix-sdk client has no authenticated user id".to_owned())?;
    let mut password_auth = uiaa::Password::new(user_id.to_owned().into(), password.to_owned());
    password_auth.session = pending_sign_out.uiaa_info.session.clone();

    match client
        .delete_devices(
            &[pending_sign_out.device_id.clone()],
            Some(AuthData::Password(password_auth)),
        )
        .await
    {
        Ok(_) => Ok(()),
        Err(error) => {
            if let Some(uiaa_info) = error.as_uiaa_response() {
                let mut pending = pending
                    .lock()
                    .expect("poisoned matrix backend pending device sign-out mutex");
                *pending = Some(PendingDeviceSignOut {
                    device_id: pending_sign_out.device_id,
                    uiaa_info: uiaa_info.clone(),
                });

                return Err(
                    uiaa_info
                        .auth_error
                        .as_ref()
                        .map(|e| e.message.clone())
                        .filter(|m| !m.is_empty())
                        .unwrap_or_else(|| "incorrect password or authentication failed".to_owned()),
                );
            }

            let mut pending = pending
                .lock()
                .expect("poisoned matrix backend pending device sign-out mutex");
            *pending = Some(pending_sign_out);
            Err(format!("failed to complete device sign-out: {error}"))
        }
    }
}
