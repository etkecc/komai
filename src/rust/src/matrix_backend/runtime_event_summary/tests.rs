// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::state::membership_change_kind_key;
use super::summarize_sync_state_event;
use matrix_sdk::ruma::events::AnySyncStateEvent;
use matrix_sdk::ruma::serde::Raw;
use matrix_sdk_ui::timeline::MembershipChange;

#[test]
fn remote_own_reaction_stays_highlighted_without_an_event_id() {
    use matrix_sdk::ruma::{MilliSecondsSinceUnixEpoch, user_id};
    use matrix_sdk_ui::timeline::{ReactionInfo, ReactionsByKeyBySender};

    let own_user = user_id!("@alice:example.org");
    let mut reactions = ReactionsByKeyBySender::default();
    reactions.entry("thumbs-up".to_owned()).or_default().insert(
        own_user.to_owned(),
        ReactionInfo { timestamp: MilliSecondsSinceUnixEpoch(1u32.into()), send_state: None },
    );

    let summary = super::messages::summarize_reaction_items(&reactions, Some(own_user));
    assert_eq!(summary[0].count, 1);
    assert!(!summary[0].self_reacted_event.is_empty());
    assert_eq!(summary[0].user_ids, vec![own_user.to_string()]);

    let summary = super::messages::summarize_reaction_items(
        &reactions, Some(user_id!("@bob:example.org")),
    );
    assert!(summary[0].self_reacted_event.is_empty());
}

#[test]
fn membership_change_kind_key_returns_join_and_left_variants() {
    assert_eq!(membership_change_kind_key(Some(MembershipChange::Joined)), "joined");
    assert_eq!(membership_change_kind_key(Some(MembershipChange::Left)), "left");
    assert_eq!(
        membership_change_kind_key(Some(MembershipChange::InvitationAccepted)),
        "invitation_accepted"
    );
    assert_eq!(membership_change_kind_key(None), "redacted");
}

fn state_event_from_json(json: &str) -> AnySyncStateEvent {
    serde_json::from_str::<Raw<AnySyncStateEvent>>(json)
        .expect("valid raw event")
        .deserialize()
        .expect("deserializable state event")
}

#[test]
fn summarize_sync_state_event_classifies_join() {
    let event = state_event_from_json(
        r#"{
            "type": "m.room.member",
            "event_id": "$e1",
            "sender": "@alice:example.com",
            "state_key": "@alice:example.com",
            "origin_server_ts": 1,
            "content": { "membership": "join", "displayname": "Alice" }
        }"#,
    );
    let s = summarize_sync_state_event(&event);
    assert_eq!(s.kind, "membership_change");
    assert_eq!(s.membership_change_kind, "joined");
    assert_eq!(s.state_event_target_user, "Alice");
    assert_eq!(s.state_event_target_user_id, "@alice:example.com");
    assert!(!s.state_event_has_sender);
}

#[test]
fn summarize_sync_state_event_classifies_kick_with_reason() {
    let event = state_event_from_json(
        r#"{
            "type": "m.room.member",
            "event_id": "$e2",
            "sender": "@mod:example.com",
            "state_key": "@spammer:example.com",
            "origin_server_ts": 2,
            "content": { "membership": "leave", "reason": "spam" },
            "unsigned": { "prev_content": { "membership": "join" } }
        }"#,
    );
    let s = summarize_sync_state_event(&event);
    assert_eq!(s.membership_change_kind, "kicked");
    assert_eq!(s.state_event_reason, "spam");
    assert!(s.state_event_has_sender);
}

#[test]
fn summarize_sync_state_event_classifies_displayname_change() {
    let event = state_event_from_json(
        r#"{
            "type": "m.room.member",
            "event_id": "$e3",
            "sender": "@alice:example.com",
            "state_key": "@alice:example.com",
            "origin_server_ts": 3,
            "content": { "membership": "join", "displayname": "Alice2" },
            "unsigned": { "prev_content": { "membership": "join", "displayname": "Alice" } }
        }"#,
    );
    let s = summarize_sync_state_event(&event);
    assert_eq!(s.kind, "profile_change");
    assert_eq!(s.membership_change_kind, "displayname");
    assert_eq!(s.state_event_detail, "Alice2");
}

#[test]
fn summarize_sync_state_event_extracts_room_name() {
    let event = state_event_from_json(
        r#"{
            "type": "m.room.name",
            "event_id": "$e4",
            "sender": "@alice:example.com",
            "state_key": "",
            "origin_server_ts": 4,
            "content": { "name": "Project Alpha" }
        }"#,
    );
    let s = summarize_sync_state_event(&event);
    assert_eq!(s.kind, "other_state");
    assert_eq!(s.matrix_event_type, "m.room.name");
    assert_eq!(s.state_event_detail, "Project Alpha");
}
