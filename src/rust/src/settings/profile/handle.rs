// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::pin::Pin;

use crate::{ffi, settings};

pub struct SettingsProfileHandle {
    profile_id: String,
    loaded: ffi::SettingsLoadedProfile,
}

impl SettingsProfileHandle {
    pub fn load_for_profile(profile_id: &str, include_session: bool) -> Self {
        let snapshot = settings::profile::load_profile_snapshot_for_profile(profile_id, include_session);

        Self {
            profile_id: profile_id.to_owned(),
            loaded: ffi::SettingsLoadedProfile {
                config: settings::ffi::ffi_loaded_config(snapshot.config),
                session: settings::ffi::ffi_loaded_session(snapshot.session),
                state: settings::ffi::ffi_loaded_state(snapshot.state),
            },
        }
    }

    pub fn snapshot(&self) -> ffi::SettingsLoadedProfile {
        settings::ffi::clone_loaded_profile(&self.loaded)
    }

    pub fn set_config_secrets_provider(mut self: Pin<&mut Self>, provider: &str) {
        self.as_mut().get_mut().loaded.config.secrets.provider = provider.to_owned();
    }

    pub fn replace_config_snapshot(
        mut self: Pin<&mut Self>,
        snapshot: &ffi::SettingsConfigSnapshot,
    ) {
        self.as_mut().get_mut().loaded.config = settings::ffi::loaded_config_from_snapshot(snapshot);
    }

    pub fn replace_session_identity(
        mut self: Pin<&mut Self>,
        user_id: &str,
        homeserver: &str,
        device_id: &str,
    ) {
        self.as_mut().get_mut().loaded.session =
            settings::ffi::loaded_session_from_identity(user_id, homeserver, device_id);
    }

    pub fn replace_state_snapshot(mut self: Pin<&mut Self>, snapshot: &ffi::SettingsStateSnapshot) {
        self.as_mut().get_mut().loaded.state = settings::ffi::loaded_state_from_snapshot(snapshot);
    }

    pub fn write_config(&self) -> bool {
        let config_path = settings::storage::config_file_path_for_profile(&self.profile_id);
        settings::ffi::write_loaded_config_to_path(&config_path, &self.loaded.config)
    }

    pub fn write_session(&self) -> bool {
        let session_path = settings::storage::session_file_path_for_profile(&self.profile_id);
        settings::ffi::write_loaded_session_to_path(&session_path, &self.loaded.session)
    }

    pub fn write_state(&self) -> bool {
        let state_path = settings::storage::state_file_path_for_profile(&self.profile_id);
        settings::ffi::write_loaded_state_to_path(&state_path, &self.loaded.state)
    }
}
