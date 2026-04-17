// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Value};

use crate::ffi::{SettingsBoolMapEntry, SettingsStringListMapEntry};

pub(super) fn bool_map(entries: &[SettingsBoolMapEntry]) -> Value {
    let mut mapping = Mapping::new();
    for entry in entries {
        mapping.insert(
            Value::String(entry.key.clone()),
            Value::Bool(entry.value),
        );
    }
    Value::Mapping(mapping)
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
