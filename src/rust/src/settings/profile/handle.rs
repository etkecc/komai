// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::pin::Pin;

use crate::{ffi, settings};

pub struct SettingsProfileHandle {
    profile_id: String,
    loaded: ffi::SettingsLoadedProfile,
    config_dirty: bool,
    session_dirty: bool,
    state_dirty: bool,
}

impl SettingsProfileHandle {
    pub fn load_for_profile(profile_id: &str, include_session: bool) -> Self {
        let snapshot = settings::profile::load_profile_snapshot_for_profile(profile_id, include_session);
        let config_dirty = snapshot.config.source_exists && snapshot.config.should_write_back;
        let session_dirty = snapshot.session.source_exists && snapshot.session.should_write_back;
        let state_dirty = snapshot.state.source_exists && snapshot.state.should_write_back;

        Self {
            profile_id: profile_id.to_owned(),
            loaded: ffi::SettingsLoadedProfile {
                config: settings::ffi::ffi_loaded_config(snapshot.config),
                session: settings::ffi::ffi_loaded_session(snapshot.session),
                state: settings::ffi::ffi_loaded_state(snapshot.state),
            },
            config_dirty,
            session_dirty,
            state_dirty,
        }
    }

    pub fn snapshot(&self) -> ffi::SettingsLoadedProfile {
        settings::ffi::clone_loaded_profile(&self.loaded)
    }

    pub fn set_config_secrets_provider(mut self: Pin<&mut Self>, provider: &str) {
        let this = self.as_mut().get_mut();
        if this.loaded.config.secrets.provider != provider {
            this.loaded.config.secrets.provider = provider.to_owned();
            this.config_dirty = true;
        }
    }

    pub fn replace_config_snapshot(
        mut self: Pin<&mut Self>,
        snapshot: &ffi::SettingsConfigSnapshot,
    ) {
        let this = self.as_mut().get_mut();
        this.loaded.config = settings::ffi::loaded_config_from_snapshot(snapshot);
        this.config_dirty = true;
    }

    pub fn replace_session_identity(
        mut self: Pin<&mut Self>,
        user_id: &str,
        homeserver: &str,
        device_id: &str,
    ) {
        let this = self.as_mut().get_mut();
        this.loaded.session = settings::ffi::loaded_session_from_identity(user_id, homeserver, device_id);
        this.session_dirty = true;
    }

    pub fn replace_state_snapshot(mut self: Pin<&mut Self>, snapshot: &ffi::SettingsStateSnapshot) {
        let this = self.as_mut().get_mut();
        this.loaded.state = settings::ffi::loaded_state_from_snapshot(snapshot);
        this.state_dirty = true;
    }

    pub fn write_config(&mut self) -> bool {
        let config_path = settings::storage::config_file_path_for_profile(&self.profile_id);
        let saved = settings::ffi::write_loaded_config_to_path(&config_path, &self.loaded.config);
        if saved {
            self.config_dirty = false;
        }
        saved
    }

    pub fn write_session(&mut self) -> bool {
        let session_path = settings::storage::session_file_path_for_profile(&self.profile_id);
        let saved = settings::ffi::write_loaded_session_to_path(&session_path, &self.loaded.session);
        if saved {
            self.session_dirty = false;
        }
        saved
    }

    pub fn write_state(&mut self) -> bool {
        let state_path = settings::storage::state_file_path_for_profile(&self.profile_id);
        let saved = settings::ffi::write_loaded_state_to_path(&state_path, &self.loaded.state);
        if saved {
            self.state_dirty = false;
        }
        saved
    }

    pub fn flush(
        mut self: Pin<&mut Self>,
        write_config: bool,
        write_session: bool,
        write_state: bool,
    ) -> ffi::SettingsProfileFlushResult {
        let this = self.as_mut().get_mut();

        let config_attempted = write_config && this.config_dirty;
        let session_attempted = write_session && this.session_dirty;
        let state_attempted = write_state && this.state_dirty;

        let config_saved = if config_attempted {
            this.write_config()
        } else {
            false
        };
        let session_saved = if session_attempted {
            this.write_session()
        } else {
            false
        };
        let state_saved = if state_attempted {
            this.write_state()
        } else {
            false
        };

        ffi::SettingsProfileFlushResult {
            config_attempted,
            config_saved,
            session_attempted,
            session_saved,
            state_attempted,
            state_saved,
        }
    }
}
