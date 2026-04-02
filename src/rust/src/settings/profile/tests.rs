// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{handle::SettingsProfileHandle, load_profile_snapshot};

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

#[test]
fn handle_prepare_for_load_picks_file_provider_when_secure_backend_is_unavailable() {
    let mut handle = SettingsProfileHandle::empty_for_test("__test__");

    std::pin::Pin::new(&mut handle).prepare_for_load(true, false);
    let snapshot = handle.snapshot();

    assert_eq!(snapshot.config.secrets.provider, "file");
    assert!(snapshot.uses_file_secrets_provider);
    assert!(snapshot.startup_secrets_provider_changed);
    assert!(snapshot.secrets_provider_fallback_warning_visible);
}

#[test]
fn handle_prepare_for_load_keeps_provider_when_persisted_session_identity_exists() {
    let mut loaded = SettingsProfileHandle::empty_for_test("__test__").snapshot();
    loaded.config.source_exists = true;
    let mut handle = SettingsProfileHandle::from_loaded("__test__", loaded);
    std::pin::Pin::new(&mut handle).replace_session_identity("@u:hs", "https://hs", "DEV");

    std::pin::Pin::new(&mut handle).prepare_for_load(true, false);
    let snapshot = handle.snapshot();

    assert_eq!(snapshot.config.secrets.provider, "");
    assert!(!snapshot.uses_file_secrets_provider);
    assert!(!snapshot.startup_secrets_provider_changed);
    assert!(!snapshot.secrets_provider_fallback_warning_visible);
}
