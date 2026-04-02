// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
