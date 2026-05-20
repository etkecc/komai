// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::*;
use matrix_sdk::ruma::{OwnedUserId, room_version_rules::AuthorizationRules, user_id};

fn make_content(
    users: &[(&OwnedUserId, i64)],
    users_default: i64,
) -> RoomPowerLevelsEventContent {
    let mut content = RoomPowerLevelsEventContent::new(&AuthorizationRules::V1);
    content.users_default = users_default.try_into().unwrap();
    for (user_id, level) in users {
        content
            .users
            .insert((*user_id).clone(), (*level).try_into().unwrap());
    }
    content
}

#[test]
fn single_user_promoted() {
    let alice = user_id!("@alice:example.com").to_owned();
    let prev = make_content(&[(&alice, 0)], 0);
    let curr = make_content(&[(&alice, 100)], 0);

    let changes = diff_power_level_users(&curr, &prev);
    assert_eq!(
        changes,
        vec![PowerLevelChange {
            user_id: alice.to_string(),
            old_level: 0,
            new_level: 100,
        }]
    );
}

#[test]
fn single_user_demoted() {
    let bob = user_id!("@bob:example.com").to_owned();
    let prev = make_content(&[(&bob, 100)], 0);
    let curr = make_content(&[(&bob, 50)], 0);

    let changes = diff_power_level_users(&curr, &prev);
    assert_eq!(
        changes,
        vec![PowerLevelChange {
            user_id: bob.to_string(),
            old_level: 100,
            new_level: 50,
        }]
    );
}

#[test]
fn multiple_users_changed() {
    let alice = user_id!("@alice:example.com").to_owned();
    let bob = user_id!("@bob:example.com").to_owned();
    let prev = make_content(&[(&alice, 0), (&bob, 100)], 0);
    let curr = make_content(&[(&alice, 100), (&bob, 50)], 0);

    let changes = diff_power_level_users(&curr, &prev);
    // BTreeSet iterates in sorted order, so @alice comes before @bob.
    assert_eq!(changes.len(), 2);
    assert_eq!(
        changes[0],
        PowerLevelChange {
            user_id: alice.to_string(),
            old_level: 0,
            new_level: 100,
        }
    );
    assert_eq!(
        changes[1],
        PowerLevelChange {
            user_id: bob.to_string(),
            old_level: 100,
            new_level: 50,
        }
    );
}

#[test]
fn user_added_to_map() {
    // User wasn't in prev map — old level comes from prev users_default.
    let carol = user_id!("@carol:example.com").to_owned();
    let prev = make_content(&[], 0);
    let curr = make_content(&[(&carol, 50)], 0);

    let changes = diff_power_level_users(&curr, &prev);
    assert_eq!(
        changes,
        vec![PowerLevelChange {
            user_id: carol.to_string(),
            old_level: 0,
            new_level: 50,
        }]
    );
}

#[test]
fn user_removed_from_map() {
    // User was in prev map but not in curr — new level comes from curr users_default.
    let dave = user_id!("@dave:example.com").to_owned();
    let prev = make_content(&[(&dave, 100)], 0);
    let curr = make_content(&[], 0);

    let changes = diff_power_level_users(&curr, &prev);
    assert_eq!(
        changes,
        vec![PowerLevelChange {
            user_id: dave.to_string(),
            old_level: 100,
            new_level: 0,
        }]
    );
}

#[test]
fn no_user_changes() {
    // Only other fields changed, users stayed the same.
    let alice = user_id!("@alice:example.com").to_owned();
    let prev = make_content(&[(&alice, 100)], 0);
    let curr = make_content(&[(&alice, 100)], 0);

    let changes = diff_power_level_users(&curr, &prev);
    assert!(changes.is_empty());
}

#[test]
fn sender_changed_own_level() {
    // Sender promoting themselves — same logic, just a self-change.
    let eve = user_id!("@eve:example.com").to_owned();
    let prev = make_content(&[(&eve, 50)], 0);
    let curr = make_content(&[(&eve, 100)], 0);

    let changes = diff_power_level_users(&curr, &prev);
    assert_eq!(
        changes,
        vec![PowerLevelChange {
            user_id: eve.to_string(),
            old_level: 50,
            new_level: 100,
        }]
    );
}

#[test]
fn users_default_changed_no_false_positive() {
    // User removed from map but new default matches old explicit level — no change.
    let frank = user_id!("@frank:example.com").to_owned();
    let prev = make_content(&[(&frank, 50)], 0);
    let curr = make_content(&[], 50); // default is now 50

    let changes = diff_power_level_users(&curr, &prev);
    assert!(changes.is_empty());
}

#[test]
fn users_default_changed_causes_change() {
    // User removed from map and new default differs from old explicit level.
    let grace = user_id!("@grace:example.com").to_owned();
    let prev = make_content(&[(&grace, 100)], 0);
    let curr = make_content(&[], 50); // default is 50, not 100

    let changes = diff_power_level_users(&curr, &prev);
    assert_eq!(
        changes,
        vec![PowerLevelChange {
            user_id: grace.to_string(),
            old_level: 100,
            new_level: 50,
        }]
    );
}

#[test]
fn empty_maps_no_changes() {
    let prev = make_content(&[], 0);
    let curr = make_content(&[], 0);

    let changes = diff_power_level_users(&curr, &prev);
    assert!(changes.is_empty());
}

// ── Server ACL diff tests ──────────────────────────────────────────

fn make_acl(
    allow: &[&str],
    deny: &[&str],
    allow_ip_literals: bool,
) -> RoomServerAclEventContent {
    RoomServerAclEventContent::new(
        allow_ip_literals,
        allow.iter().map(|s| (*s).to_owned()).collect(),
        deny.iter().map(|s| (*s).to_owned()).collect(),
    )
}

#[test]
fn acl_server_added_to_deny() {
    let prev = make_acl(&["*"], &[], true);
    let curr = make_acl(&["*"], &["evil.org"], true);

    let change = diff_server_acl(&curr, &prev);
    assert!(change.allowed_added.is_empty());
    assert!(change.allowed_removed.is_empty());
    assert_eq!(change.denied_added, vec!["evil.org"]);
    assert!(change.denied_removed.is_empty());
    assert_eq!(change.ip_literals_changed, None);
}

#[test]
fn acl_server_removed_from_deny() {
    let prev = make_acl(&["*"], &["evil.org"], true);
    let curr = make_acl(&["*"], &[], true);

    let change = diff_server_acl(&curr, &prev);
    assert!(change.denied_added.is_empty());
    assert_eq!(change.denied_removed, vec!["evil.org"]);
}

#[test]
fn acl_allow_list_changed() {
    let prev = make_acl(&["*"], &[], true);
    let curr = make_acl(&["example.com", "matrix.org"], &[], true);

    let change = diff_server_acl(&curr, &prev);
    assert_eq!(change.allowed_added, vec!["example.com", "matrix.org"]);
    assert_eq!(change.allowed_removed, vec!["*"]);
}

#[test]
fn acl_ip_literals_toggled() {
    let prev = make_acl(&["*"], &[], true);
    let curr = make_acl(&["*"], &[], false);

    let change = diff_server_acl(&curr, &prev);
    assert_eq!(change.ip_literals_changed, Some(false));
    assert!(change.allowed_added.is_empty());
    assert!(change.denied_added.is_empty());
}

#[test]
fn acl_no_changes() {
    let prev = make_acl(&["*"], &["evil.org"], false);
    let curr = make_acl(&["*"], &["evil.org"], false);

    let change = diff_server_acl(&curr, &prev);
    assert!(change.is_empty());
}

#[test]
fn acl_multiple_changes() {
    let prev = make_acl(&["*"], &["old-spam.net"], true);
    let curr = make_acl(&["*"], &["new-spam.org", "evil.com"], false);

    let change = diff_server_acl(&curr, &prev);
    assert!(change.allowed_added.is_empty());
    assert!(change.allowed_removed.is_empty());
    assert_eq!(change.denied_added, vec!["evil.com", "new-spam.org"]);
    assert_eq!(change.denied_removed, vec!["old-spam.net"]);
    assert_eq!(change.ip_literals_changed, Some(false));
    assert_eq!(change.total_changes(), 4);
}
