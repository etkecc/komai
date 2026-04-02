// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::BTreeMap;

use serde_yaml_ng::{Mapping, Value};

use crate::ffi::SettingsStringMapEntry;

use super::storage;

fn ordered_map(entries: &[SettingsStringMapEntry]) -> BTreeMap<String, String> {
    entries
        .iter()
        .map(|entry| (entry.key.clone(), entry.value.clone()))
        .collect()
}

fn encode_yaml_value(value: &Value) -> String {
    let mut serialized = serde_yaml_ng::to_string(value).unwrap_or_default();
    if let Some(rest) = serialized.strip_prefix("---\n") {
        serialized = rest.to_owned();
    }
    serialized
}

fn parse_yaml_root(serialized: &str) -> Option<Value> {
    if serialized.trim().is_empty() {
        return None;
    }

    serde_yaml_ng::from_str::<Value>(serialized).ok()
}

fn decode_string_map_value(value: Option<&Value>) -> Vec<SettingsStringMapEntry> {
    let Some(Value::Mapping(mapping)) = value else {
        return Vec::new();
    };

    let mut result = Vec::new();
    for (key, value) in mapping {
        let (Value::String(key), Value::String(value)) = (key, value) else {
            continue;
        };
        result.push(SettingsStringMapEntry {
            key: key.clone(),
            value: value.clone(),
        });
    }

    result.sort_by(|left, right| left.key.cmp(&right.key));
    result
}

pub fn encode_string_map_yaml(entries: &[SettingsStringMapEntry]) -> String {
    let value = serde_yaml_ng::to_value(ordered_map(entries)).unwrap_or(Value::Mapping(Mapping::new()));
    encode_yaml_value(&value)
}

pub fn decode_string_map_yaml(serialized: &str) -> Vec<SettingsStringMapEntry> {
    decode_string_map_value(parse_yaml_root(serialized).as_ref())
}

pub fn encode_named_string_map_yaml(root_key: &str, entries: &[SettingsStringMapEntry]) -> String {
    let mut root = Mapping::new();
    root.insert(
        Value::String(root_key.to_owned()),
        serde_yaml_ng::to_value(ordered_map(entries)).unwrap_or(Value::Mapping(Mapping::new())),
    );
    encode_yaml_value(&Value::Mapping(root))
}

pub fn decode_named_string_map_yaml(
    serialized: &str,
    root_key: &str,
) -> Vec<SettingsStringMapEntry> {
    let root = parse_yaml_root(serialized);
    let Some(Value::Mapping(mapping)) = root.as_ref() else {
        return Vec::new();
    };

    decode_string_map_value(mapping.get(Value::String(root_key.to_owned())))
}

pub fn load_named_string_map_from_path(
    path: &str,
    label: &str,
    root_key: &str,
) -> Vec<SettingsStringMapEntry> {
    decode_named_string_map_yaml(&storage::read_text_file(path, label), root_key)
}

pub fn write_named_string_map_to_path(
    path: &str,
    root_key: &str,
    entries: &[SettingsStringMapEntry],
    owner_read_write_only: bool,
) -> bool {
    storage::write_text_file(
        path,
        &encode_named_string_map_yaml(root_key, entries),
        owner_read_write_only,
    )
}

#[cfg(test)]
mod tests;
