// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi::{SettingsStateSnapshot, SettingsStringMapEntry};

use super::{
    encode_state_yaml, load_state_snapshot, DEFAULT_BADGES_HIDDEN_FILTERS,
    DEFAULT_COMMUNITIES_WIDTH, DEFAULT_ROOM_LIST_WIDTH, DEFAULT_WINDOW_HEIGHT, DEFAULT_WINDOW_WIDTH,
};

fn entry(key: &str, value: &str) -> SettingsStringMapEntry {
    SettingsStringMapEntry {
        key: key.to_owned(),
        value: value.to_owned(),
    }
}

#[test]
fn state_snapshot_loads_defaults_and_migrates() {
    let loaded = load_state_snapshot("");

    assert_eq!(loaded.window_width, DEFAULT_WINDOW_WIDTH);
    assert_eq!(loaded.window_height, DEFAULT_WINDOW_HEIGHT);
    assert_eq!(loaded.navigation_room_list_width_px, DEFAULT_ROOM_LIST_WIDTH);
    assert_eq!(loaded.navigation_communities_width_px, DEFAULT_COMMUNITIES_WIDTH);
    assert_eq!(
        loaded.badges_hidden_filters,
        DEFAULT_BADGES_HIDDEN_FILTERS
            .iter()
            .map(|value| (*value).to_owned())
            .collect::<Vec<_>>()
    );
    assert_eq!(loaded.sponsoring_status, "visible");
    assert!(loaded.should_write_back);
}

#[test]
fn state_snapshot_loads_lists_and_maps() {
    let loaded = load_state_snapshot(
        "navigation:\n  communities:\n    filtering:\n      current: people\n      global_excludes:\n        - one\n        - two\n      badges_hidden:\n        - x\n      collapsed_spaces:\n        - \"!space:hs\"\ncomposer:\n  drafts:\n    by_room:\n      \"!room:hs\": draft\n",
    );

    assert_eq!(loaded.current_filter_id, "people");
    assert_eq!(loaded.global_excludes, vec!["one".to_owned(), "two".to_owned()]);
    assert_eq!(loaded.badges_hidden_filters, vec!["x".to_owned()]);
    assert_eq!(loaded.collapsed_spaces, vec!["!space:hs".to_owned()]);
    assert_eq!(loaded.composer_drafts_by_room, vec![entry("!room:hs", "draft")]);
}

#[test]
fn state_yaml_roundtrip() {
    let encoded = encode_state_yaml(&SettingsStateSnapshot {
        window_width: 1200,
        window_height: 800,
        navigation_room_list_width_px: 320,
        navigation_communities_width_px: 240,
        current_filter_id: "people".to_owned(),
        current_room_id: "!room:hs".to_owned(),
        global_excludes: vec!["global".to_owned()],
        badges_hidden_filters: vec!["x".to_owned()],
        hidden_pins: vec!["!pin".to_owned()],
        hidden_widgets: vec!["!widget".to_owned()],
        collapsed_spaces: vec!["!space:hs".to_owned()],
        hidden_spaces: vec!["!hidden:hs".to_owned()],
        open_tabs: vec!["!tab1:hs".to_owned(), "!tab2:hs".to_owned()],
        pinned_tabs: vec!["!tab1:hs".to_owned()],
        composer_drafts_by_room: vec![entry("!room:hs", "draft")],
        sponsoring_status: "sponsoring".to_owned(),
    });
    let loaded = load_state_snapshot(&encoded);

    assert_eq!(loaded.window_width, 1200);
    assert_eq!(loaded.window_height, 800);
    assert_eq!(loaded.current_filter_id, "people");
    assert_eq!(loaded.current_room_id, "!room:hs");
    assert_eq!(loaded.open_tabs, vec!["!tab1:hs".to_owned(), "!tab2:hs".to_owned()]);
    assert_eq!(loaded.pinned_tabs, vec!["!tab1:hs".to_owned()]);
    assert_eq!(loaded.composer_drafts_by_room, vec![entry("!room:hs", "draft")]);
    assert_eq!(loaded.sponsoring_status, "sponsoring");
}
