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
    secrets_dirty: bool,
    state_dirty: bool,
}

impl SettingsProfileHandle {
    pub(crate) fn from_loaded(profile_id: &str, loaded: ffi::SettingsLoadedProfile) -> Self {
        let config_dirty = loaded.config.source_exists && loaded.config.should_write_back;
        let session_dirty = loaded.session.source_exists && loaded.session.should_write_back;
        let state_dirty = loaded.state.source_exists && loaded.state.should_write_back;

        Self {
            profile_id: profile_id.to_owned(),
            loaded,
            config_dirty,
            session_dirty,
            secrets_dirty: false,
            state_dirty,
        }
    }

    fn uses_file_secrets_provider(&self) -> bool {
        self.loaded.config.secrets.provider == "file"
    }

    fn has_persisted_session_identity(&self) -> bool {
        !self.loaded.session.user_id.trim().is_empty()
            && !self.loaded.session.device_id.trim().is_empty()
            && !self.loaded.session.homeserver.trim().is_empty()
    }

    fn has_active_session(&self) -> bool {
        self.has_persisted_session_identity() && !self.loaded.secrets.access_token.trim().is_empty()
    }

    fn preferred_secrets_provider(secure_backend_available: bool) -> &'static str {
        if secure_backend_available {
            "secret_service"
        } else {
            "file"
        }
    }

    fn refresh_loaded_config_yaml(&mut self) {
        self.loaded.config.serialized_yaml =
            settings::config::encode_config_yaml(&settings::ffi::loaded_config_to_snapshot(
                &self.loaded.config,
            ));
    }

    pub fn load_for_profile(profile_id: &str, include_session: bool) -> Self {
        let snapshot = settings::profile::load_profile_snapshot_for_profile(profile_id, include_session);
        let uses_file_secrets_provider =
            snapshot.config.config.secrets.provider.to_storage_string() == "file";
        Self::from_loaded(
            profile_id,
            ffi::SettingsLoadedProfile {
                config: settings::ffi::ffi_loaded_config(snapshot.config),
                session: settings::ffi::ffi_loaded_session(snapshot.session),
                state: settings::ffi::ffi_loaded_state(snapshot.state),
                secrets: settings::secrets::load_profile_secrets(profile_id, uses_file_secrets_provider),
                uses_file_secrets_provider,
                startup_secrets_provider_changed: false,
                secrets_provider_fallback_warning_visible: false,
            },
        )
    }

    #[cfg(test)]
    pub(crate) fn empty_for_test(profile_id: &str) -> Self {
        Self::from_loaded(
            profile_id,
            ffi::SettingsLoadedProfile {
                config: settings::ffi::ffi_loaded_config(settings::config::load_config_snapshot("")),
                session: settings::ffi::ffi_loaded_session(settings::session::load_session_snapshot("")),
                state: settings::ffi::ffi_loaded_state(settings::state::load_state_snapshot("")),
                secrets: ffi::SettingsSecretsPayload {
                    access_token: String::new(),
                    secrets: Vec::new(),
                    had_stale_values: false,
                },
                uses_file_secrets_provider: false,
                startup_secrets_provider_changed: false,
                secrets_provider_fallback_warning_visible: false,
            },
        )
    }

    pub fn snapshot(&self) -> ffi::SettingsLoadedProfile {
        settings::ffi::clone_loaded_profile(&self.loaded)
    }

    pub fn prepare_for_load(mut self: Pin<&mut Self>, full_load: bool, secure_backend_available: bool) {
        let this = self.as_mut().get_mut();
        this.loaded.startup_secrets_provider_changed = false;
        this.loaded.secrets_provider_fallback_warning_visible = false;

        let preferred_provider = Self::preferred_secrets_provider(secure_backend_available);
        let should_switch_provider = !this.loaded.config.source_exists
            || (full_load && !this.has_active_session() && !this.has_persisted_session_identity());
        if should_switch_provider && this.loaded.config.secrets.provider != preferred_provider {
            this.loaded.config.secrets.provider = preferred_provider.to_owned();
            this.loaded.uses_file_secrets_provider = preferred_provider == "file";
            this.loaded.startup_secrets_provider_changed = true;
            this.config_dirty = true;
            this.refresh_loaded_config_yaml();
        } else {
            this.loaded.uses_file_secrets_provider = this.uses_file_secrets_provider();
        }

        if full_load && !this.has_active_session() && !this.has_persisted_session_identity() {
            this.loaded.secrets_provider_fallback_warning_visible =
                this.loaded.uses_file_secrets_provider && !secure_backend_available;
        }
    }

    pub fn replace_config_snapshot(
        mut self: Pin<&mut Self>,
        snapshot: &ffi::SettingsConfigSnapshot,
    ) {
        let this = self.as_mut().get_mut();
        let updated = settings::ffi::loaded_config_from_snapshot(snapshot);
        let changed = this.loaded.config.serialized_yaml != updated.serialized_yaml;
        this.loaded.config = updated;
        this.config_dirty = this.config_dirty || changed;
    }

    pub fn replace_session_identity(
        mut self: Pin<&mut Self>,
        user_id: &str,
        homeserver: &str,
        device_id: &str,
    ) {
        let this = self.as_mut().get_mut();
        let updated = settings::ffi::loaded_session_from_identity(user_id, homeserver, device_id);
        let changed = this.loaded.session.serialized_yaml != updated.serialized_yaml;
        this.loaded.session = updated;
        this.session_dirty = this.session_dirty || changed;
    }

    pub fn replace_state_snapshot(mut self: Pin<&mut Self>, snapshot: &ffi::SettingsStateSnapshot) {
        let this = self.as_mut().get_mut();
        let updated = settings::ffi::loaded_state_from_snapshot(snapshot);
        let changed = this.loaded.state.serialized_yaml != updated.serialized_yaml;
        this.loaded.state = updated;
        this.state_dirty = this.state_dirty || changed;
    }

    pub fn replace_secrets_payload(
        mut self: Pin<&mut Self>,
        access_token: &str,
        entries: &[ffi::SettingsStringMapEntry],
    ) {
        let this = self.as_mut().get_mut();
        let updated = ffi::SettingsSecretsPayload {
            access_token: access_token.to_owned(),
            secrets: entries.to_vec(),
            had_stale_values: false,
        };
        let changed = this.loaded.secrets.access_token != updated.access_token
            || this.loaded.secrets.secrets != updated.secrets;
        this.loaded.secrets = updated;
        this.secrets_dirty = this.secrets_dirty || changed;
    }

    pub fn write_config(&mut self) -> bool {
        let config_path = settings::storage::config_file_path_for_profile(&self.profile_id);
        let saved = settings::ffi::write_loaded_config_to_path(&config_path, &self.loaded.config);
        if saved {
            self.config_dirty = false;
            self.loaded.config.source_exists = true;
            self.loaded.config.should_write_back = false;
        }
        saved
    }

    pub fn write_session(&mut self) -> bool {
        let session_path = settings::storage::session_file_path_for_profile(&self.profile_id);
        let saved = settings::ffi::write_loaded_session_to_path(&session_path, &self.loaded.session);
        if saved {
            self.session_dirty = false;
            self.loaded.session.source_exists = true;
            self.loaded.session.should_write_back = false;
        }
        saved
    }

    pub fn write_state(&mut self) -> bool {
        let state_path = settings::storage::state_file_path_for_profile(&self.profile_id);
        let saved = settings::ffi::write_loaded_state_to_path(&state_path, &self.loaded.state);
        if saved {
            self.state_dirty = false;
            self.loaded.state.source_exists = true;
            self.loaded.state.should_write_back = false;
        }
        saved
    }

    pub fn write_secrets(&mut self) -> bool {
        let uses_file_secrets_provider = self.loaded.config.secrets.provider == "file";
        let saved = settings::secrets::save_profile_secrets(
            &self.profile_id,
            uses_file_secrets_provider,
            &self.loaded.secrets.access_token,
            self.loaded.secrets.secrets.as_slice(),
        );
        if saved {
            self.secrets_dirty = false;
            self.loaded.secrets.had_stale_values = false;
        }
        saved
    }

    pub fn flush(
        mut self: Pin<&mut Self>,
        write_config: bool,
        write_session: bool,
        write_secrets: bool,
        write_state: bool,
    ) -> ffi::SettingsProfileFlushResult {
        let this = self.as_mut().get_mut();
        let config_path = settings::storage::config_file_path_for_profile(&this.profile_id);
        let session_path = settings::storage::session_file_path_for_profile(&this.profile_id);
        let state_path = settings::storage::state_file_path_for_profile(&this.profile_id);

        let config_attempted = write_config
            && (this.config_dirty
                || (this.loaded.config.source_exists && !settings::storage::path_exists(&config_path)));
        let session_attempted = write_session
            && (this.session_dirty
                || (this.loaded.session.source_exists
                    && !settings::storage::path_exists(&session_path)));
        let secrets_attempted = write_secrets;
        let state_attempted = write_state
            && (this.state_dirty
                || (this.loaded.state.source_exists && !settings::storage::path_exists(&state_path)));

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
        let secrets_saved = if secrets_attempted {
            this.write_secrets()
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
            secrets_attempted,
            secrets_saved,
            state_attempted,
            state_saved,
        }
    }
}
