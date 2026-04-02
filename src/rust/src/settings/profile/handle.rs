// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::pin::Pin;

use crate::{ffi, settings};

pub struct SettingsProfileHandle {
    config_path: String,
    session_path: String,
    state_path: String,
    loaded: ffi::SettingsLoadedProfile,
}

impl SettingsProfileHandle {
    pub fn load_for_profile(profile_id: &str, include_session: bool) -> Self {
        let config_path = settings::storage::config_file_path_for_profile(profile_id);
        let session_path = settings::storage::session_file_path_for_profile(profile_id);
        let state_path = settings::storage::state_file_path_for_profile(profile_id);

        Self::load_from_paths(&config_path, &session_path, &state_path, include_session)
    }

    pub fn load_from_paths(
        config_path: &str,
        session_path: &str,
        state_path: &str,
        include_session: bool,
    ) -> Self {
        let snapshot =
            settings::profile::load_profile_snapshot_from_paths(config_path, session_path, state_path, include_session);

        Self {
            config_path: config_path.to_owned(),
            session_path: session_path.to_owned(),
            state_path: state_path.to_owned(),
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
        settings::ffi::write_loaded_config_to_path(&self.config_path, &self.loaded.config)
    }

    pub fn write_session(&self) -> bool {
        settings::ffi::write_loaded_session_to_path(&self.session_path, &self.loaded.session)
    }

    pub fn write_state(&self) -> bool {
        settings::ffi::write_loaded_state_to_path(&self.state_path, &self.loaded.state)
    }
}
