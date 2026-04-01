// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Number, Value};

pub(crate) fn empty_mapping() -> Value {
    Value::Mapping(Mapping::new())
}

pub(crate) fn parse_root(serialized: &str) -> Value {
    if serialized.trim().is_empty() {
        return empty_mapping();
    }

    match serde_yaml_ng::from_str::<Value>(serialized) {
        Ok(Value::Mapping(mapping)) => Value::Mapping(mapping),
        Ok(_) | Err(_) => empty_mapping(),
    }
}

pub(crate) fn value_at_path<'a>(root: &'a Value, path: &[&str]) -> Option<&'a Value> {
    let mut current = root;

    for key in path {
        current = match current {
            Value::Mapping(map) => map.get(Value::String((*key).to_owned()))?,
            _ => return None,
        };
    }

    Some(current)
}

pub(crate) fn read_schema_version(root: &Value, path: &[&str]) -> i32 {
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

pub(crate) fn set_value(root: &mut Value, path: &[&str], value: Value) {
    if path.is_empty() {
        *root = value;
        return;
    }

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

pub(crate) fn serialize_yaml(root: &Value) -> String {
    let mut serialized = serde_yaml_ng::to_string(root).unwrap_or_default();
    if let Some(rest) = serialized.strip_prefix("---\n") {
        serialized = rest.to_owned();
    }
    serialized
}

pub(crate) fn number_value(value: i32) -> Value {
    Value::Number(Number::from(value))
}
