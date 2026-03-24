// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_json::Value;

use crate::ffi;

#[derive(Default)]
pub struct PersistedLegacyMatrixSession {
    pub user_id: String,
    pub device_id: String,
    pub homeserver_url: String,
    pub access_token: String,
}

impl PersistedLegacyMatrixSession {
    pub fn has_complete_session(&self) -> bool {
        !self.user_id.trim().is_empty()
            && !self.device_id.trim().is_empty()
            && !self.homeserver_url.trim().is_empty()
            && !self.access_token.trim().is_empty()
    }
}

pub fn load_persisted_legacy_matrix_session(
    profile_id: &str,
) -> Result<PersistedLegacyMatrixSession, String> {
    let raw = ffi::matrix_legacy_session_json(profile_id);
    let value: Value = serde_json::from_str(&raw)
        .map_err(|e| format!("failed to parse legacy session snapshot JSON: {e}"))?;
    let object = value
        .as_object()
        .ok_or_else(|| "legacy session snapshot JSON is not an object".to_owned())?;

    Ok(PersistedLegacyMatrixSession {
        user_id: object
            .get("user_id")
            .and_then(Value::as_str)
            .unwrap_or_default()
            .to_owned(),
        device_id: object
            .get("device_id")
            .and_then(Value::as_str)
            .unwrap_or_default()
            .to_owned(),
        homeserver_url: object
            .get("homeserver_url")
            .and_then(Value::as_str)
            .unwrap_or_default()
            .to_owned(),
        access_token: object
            .get("access_token")
            .and_then(Value::as_str)
            .unwrap_or_default()
            .to_owned(),
    })
}
