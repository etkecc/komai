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
mod tests {
    use super::load_profile_snapshot;

    #[test]
    fn profile_snapshot_loads_each_section() {
        let loaded = load_profile_snapshot(
            "ui:\n  theme:\n    slug: komai-light\n",
            "session:\n  account:\n    user_id: '@u:hs'\n",
            "ui:\n  window:\n    width_px: 1200\n",
        );

        assert_eq!(loaded.config.config.ui.theme.slug, "komai-light");
        assert_eq!(loaded.session.user_id, "@u:hs");
        assert_eq!(loaded.state.window_width, 1200);
    }
}
