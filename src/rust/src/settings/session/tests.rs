// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{encode_session_yaml, load_session_snapshot};

#[test]
fn session_snapshot_migrates_and_normalizes() {
    let loaded = load_session_snapshot(
        "session:\n  account:\n    user_id: \" @u:hs \"\n    homeserver: \" https://hs \"\n  device:\n    id: \" DEV \"\n",
    );

    assert_eq!(loaded.user_id, "@u:hs");
    assert_eq!(loaded.homeserver, "https://hs");
    assert_eq!(loaded.device_id, "DEV");
    assert_eq!(loaded.source_version, 0);
    assert_eq!(loaded.migrated_version, 1);
    assert!(loaded.should_write_back);
}

#[test]
fn session_snapshot_respects_future_version() {
    let loaded = load_session_snapshot("meta:\n  settings_schema_version: 3\n");

    assert!(loaded.had_future_version);
    assert!(!loaded.should_write_back);
    assert_eq!(loaded.migrated_version, 3);
}

#[test]
fn encoded_session_yaml_contains_schema_and_identity() {
    let encoded = encode_session_yaml("@u:hs", "https://hs", "DEV");
    let loaded = load_session_snapshot(&encoded);

    assert_eq!(loaded.user_id, "@u:hs");
    assert_eq!(loaded.homeserver, "https://hs");
    assert_eq!(loaded.device_id, "DEV");
    assert_eq!(loaded.migrated_version, 1);
}
