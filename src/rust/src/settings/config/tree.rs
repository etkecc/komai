// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Value};

use crate::ffi::{
    SettingsBoolMapEntry, SettingsConfigTranscriptionByRoomEntry, SettingsStringListMapEntry,
};

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

/// Encode the per-room transcription overrides map to YAML. Each room's
/// inner mapping contains only the fields with `has_*` true, so a partial
/// override (e.g. only `model` set) round-trips without phantom siblings.
/// Rooms with no overrides at all are dropped from the output.
pub(super) fn transcription_by_room_map(
    entries: &[SettingsConfigTranscriptionByRoomEntry],
) -> Value {
    let mut mapping = Mapping::new();
    for entry in entries {
        let mut room = Mapping::new();
        if entry.has_provider {
            room.insert(
                Value::String("provider".to_owned()),
                Value::String(entry.provider.clone()),
            );
        }
        if entry.has_api_url {
            room.insert(
                Value::String("api_url".to_owned()),
                Value::String(entry.api_url.clone()),
            );
        }
        if entry.has_model {
            room.insert(
                Value::String("model".to_owned()),
                Value::String(entry.model.clone()),
            );
        }
        if entry.has_language {
            room.insert(
                Value::String("language".to_owned()),
                Value::String(entry.language.clone()),
            );
        }
        if entry.has_prompt {
            room.insert(
                Value::String("prompt".to_owned()),
                Value::String(entry.prompt.clone()),
            );
        }
        if room.is_empty() {
            continue;
        }
        mapping.insert(Value::String(entry.key.clone()), Value::Mapping(room));
    }
    Value::Mapping(mapping)
}
