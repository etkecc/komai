// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi::SettingsStringMapEntry;

use super::{
    decode_named_string_map_yaml, decode_string_map_yaml, encode_named_string_map_yaml,
    encode_string_map_yaml,
};

fn entry(key: &str, value: &str) -> SettingsStringMapEntry {
    SettingsStringMapEntry {
        key: key.to_owned(),
        value: value.to_owned(),
    }
}

#[test]
fn string_map_yaml_roundtrip() {
    let encoded = encode_string_map_yaml(&[entry("device-id", "dev123"), entry("access-token", "abcd")]);
    let decoded = decode_string_map_yaml(&encoded);

    assert_eq!(
        decoded,
        vec![entry("access-token", "abcd"), entry("device-id", "dev123")]
    );
}

#[test]
fn named_string_map_yaml_roundtrip() {
    let encoded = encode_named_string_map_yaml(
        "secrets",
        &[entry("__session.access_token", "token"), entry("other", "value")],
    );
    let decoded = decode_named_string_map_yaml(&encoded, "secrets");

    assert_eq!(
        decoded,
        vec![entry("__session.access_token", "token"), entry("other", "value")]
    );
}

#[test]
fn invalid_named_string_map_yaml_returns_empty() {
    assert!(decode_named_string_map_yaml("not: [yaml", "secrets").is_empty());
    assert!(decode_named_string_map_yaml("plain: text", "secrets").is_empty());
}
