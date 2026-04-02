// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{config, session, state, storage};

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

pub fn load_profile_snapshot_from_paths(
    config_path: &str,
    session_path: &str,
    state_path: &str,
    include_session: bool,
) -> LoadedProfile {
    let config_text = storage::read_text_file(config_path, "config");
    let state_text = storage::read_text_file(state_path, "state");
    let session_text = if include_session {
        storage::read_text_file(session_path, "session")
    } else {
        String::new()
    };

    load_profile_snapshot(&config_text, &session_text, &state_text)
}

#[cfg(test)]
mod tests;
