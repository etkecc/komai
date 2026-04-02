// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::snapshot_from_config_text;

#[test]
fn snapshot_extracts_supported_scale_factor() {
    let snapshot = snapshot_from_config_text("ui:\n  scale:\n    factor: 2.0\n");

    assert_eq!(snapshot.ui_scale_factor, Some(2.0));
}

#[test]
fn snapshot_ignores_malformed_scale_factor() {
    let snapshot = snapshot_from_config_text("ui:\n  scale:\n    factor: invalid\n");

    assert_eq!(snapshot.ui_scale_factor, None);
}

#[test]
fn snapshot_ignores_out_of_range_scale_factor() {
    let snapshot = snapshot_from_config_text("ui:\n  scale:\n    factor: 5.0\n");

    assert_eq!(snapshot.ui_scale_factor, None);
}
