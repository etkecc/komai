// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Number, Value};

use crate::ffi::SettingsStringListMapEntry;

pub(super) fn empty_mapping() -> Value {
    Value::Mapping(Mapping::new())
}

pub(super) fn parse_root(serialized: &str) -> Value {
    if serialized.trim().is_empty() {
        return empty_mapping();
    }

    match serde_yaml_ng::from_str::<Value>(serialized) {
        Ok(Value::Mapping(mapping)) => Value::Mapping(mapping),
        Ok(_) | Err(_) => empty_mapping(),
    }
}

pub(super) fn dotted_path(key: &str) -> Vec<&str> {
    key.split('.').filter(|segment| !segment.is_empty()).collect()
}

pub(super) fn value_at_path<'a>(root: &'a Value, path: &[&str]) -> Option<&'a Value> {
    let mut current = root;

    for key in path {
        current = match current {
            Value::Mapping(map) => map.get(Value::String((*key).to_owned()))?,
            _ => return None,
        };
    }

    Some(current)
}

pub(super) fn read_schema_version(root: &Value, path: &[&str]) -> i32 {
    match value_at_path(root, path) {
        Some(Value::Number(number)) => number.as_i64().unwrap_or_default().max(0) as i32,
        Some(Value::String(value)) => value.parse::<i32>().ok().unwrap_or_default().max(0),
        _ => 0,
    }
}

fn ensure_mapping<'a>(mapping: &'a mut Mapping, key: &str) -> &'a mut Mapping {
    let key_value = Value::String(key.to_owned());
    let needs_init = !matches!(mapping.get(&key_value), Some(Value::Mapping(_)));
    if needs_init {
        mapping.insert(key_value.clone(), Value::Mapping(Mapping::new()));
    }

    let Some(Value::Mapping(next)) = mapping.get_mut(&key_value) else {
        unreachable!();
    };
    next
}

pub(super) fn set_value(root: &mut Value, path: &[&str], value: Value) {
    let Value::Mapping(mapping) = root else {
        *root = Value::Mapping(Mapping::new());
        set_value(root, path, value);
        return;
    };

    let mut current = mapping;
    for segment in &path[..path.len() - 1] {
        current = ensure_mapping(current, segment);
    }
    current.insert(Value::String(path[path.len() - 1].to_owned()), value);
}

pub(super) fn mapping_as_string_list_map(
    mapping: &Mapping,
) -> Option<Vec<SettingsStringListMapEntry>> {
    let mut entries = Vec::new();
    for (key, value) in mapping {
        let Value::String(key) = key else {
            return None;
        };
        let Value::Sequence(sequence) = value else {
            return None;
        };

        let mut strings = Vec::new();
        for entry in sequence {
            let Value::String(string_value) = entry else {
                return None;
            };
            strings.push(string_value.clone());
        }

        entries.push(SettingsStringListMapEntry {
            key: key.clone(),
            values: strings,
        });
    }
    Some(entries)
}

pub(super) fn string_list_map(entries: &[SettingsStringListMapEntry]) -> Value {
    let mut mapping = Mapping::new();
    for entry in entries {
        mapping.insert(
            Value::String(entry.key.clone()),
            Value::Sequence(
                entry
                    .values
                    .iter()
                    .map(|value| Value::String(value.clone()))
                    .collect(),
            ),
        );
    }
    Value::Mapping(mapping)
}

pub(super) fn serialize_yaml(root: &Value) -> String {
    let mut serialized = serde_yaml_ng::to_string(root).unwrap_or_default();
    if let Some(rest) = serialized.strip_prefix("---\n") {
        serialized = rest.to_owned();
    }
    serialized
}

pub(super) fn number_value(value: i32) -> Value {
    Value::Number(Number::from(value))
}
