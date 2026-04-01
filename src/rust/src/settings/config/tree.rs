// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Value};

use crate::ffi::SettingsStringListMapEntry;

pub(super) fn dotted_path(key: &str) -> Vec<&str> {
    key.split('.').filter(|segment| !segment.is_empty()).collect()
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
