// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::membership_change_kind_key;
use matrix_sdk_ui::timeline::MembershipChange;

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
