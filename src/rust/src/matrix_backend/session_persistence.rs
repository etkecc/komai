// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi;

pub struct PersistedMatrixSessionSecrets {
    pub store_passphrase: String,
    pub homeserver_url: String,
    pub serialized_session: String,
}

pub fn load_persisted_session_secrets(profile_id: &str) -> PersistedMatrixSessionSecrets {
    PersistedMatrixSessionSecrets {
        store_passphrase: ffi::matrix_store_passphrase(profile_id),
        homeserver_url: ffi::matrix_homeserver_url(profile_id),
        serialized_session: ffi::matrix_serialized_session(profile_id),
    }
}

pub fn save_persisted_session_secrets(profile_id: &str, secrets: &PersistedMatrixSessionSecrets) {
    ffi::matrix_save_session_secrets(
        profile_id,
        &secrets.store_passphrase,
        &secrets.homeserver_url,
        &secrets.serialized_session,
    );
}

pub fn clear_persisted_session_secrets(profile_id: &str) {
    ffi::matrix_clear_session_secrets(profile_id);
}
