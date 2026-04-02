// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{config, session, state, storage};

mod handle;
pub use handle::SettingsProfileHandle;

pub struct LoadedProfile {
    pub config: config::LoadedConfig,
    pub session: session::LoadedSession,
    pub state: state::LoadedState,
}

pub fn load_profile_snapshot(
    config_text: &str,
    session_text: &str,
    state_text: &str,
) -> LoadedProfile {
    LoadedProfile {
        config: config::load_config_snapshot(config_text),
        session: session::load_session_snapshot(session_text),
        state: state::load_state_snapshot(state_text),
    }
}

pub fn load_profile_snapshot_for_profile(profile_id: &str, include_session: bool) -> LoadedProfile {
    let config_path = storage::config_file_path_for_profile(profile_id);
    let state_path = storage::state_file_path_for_profile(profile_id);
    let session_path = storage::session_file_path_for_profile(profile_id);
    let config_exists = storage::path_exists(&config_path);
    let state_exists = storage::path_exists(&state_path);
    let session_exists = include_session && storage::path_exists(&session_path);

    let config_text = storage::read_text_file(&config_path, "config");
    let state_text = storage::read_text_file(&state_path, "state");
    let session_text = if include_session {
        storage::read_text_file(&session_path, "session")
    } else {
        String::new()
    };

    let mut loaded = load_profile_snapshot(&config_text, &session_text, &state_text);
    loaded.config.source_exists = config_exists;
    loaded.state.source_exists = state_exists;
    loaded.session.source_exists = session_exists;
    loaded
}

#[cfg(test)]
mod tests;
