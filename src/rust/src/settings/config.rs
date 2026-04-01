// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Number, Value};

use crate::ffi::{
    SettingsConfigSnapshot, SettingsConfigValue, SettingsConfigValueKind, SettingsStringListMapEntry,
};

const CURRENT_CONFIG_SCHEMA_VERSION: i32 = 1;
const CONFIG_SCHEMA_VERSION_PATH: [&str; 2] = ["meta", "settings_schema_version"];

#[derive(Clone, Debug, Default)]
pub struct StoredConfig {
    pub ui_scale_factor: Option<f32>,
}

pub struct LoadedConfig {
    pub values: Vec<SettingsConfigValue>,
    pub source_version: i32,
    pub migrated_version: i32,
    pub had_future_version: bool,
    pub had_unsupported_path: bool,
    pub should_write_back: bool,
    pub serialized_yaml: String,
}

pub fn parse_config_text(config_text: &str) -> StoredConfig {
    let root = match serde_yaml_ng::from_str::<Value>(config_text) {
        Ok(root) => root,
        Err(_) => return StoredConfig::default(),
    };

    StoredConfig {
        ui_scale_factor: value_at_path(&root, &["ui", "scale", "factor"])
            .and_then(parse_scalar_f32)
            .and_then(normalize_scale_factor),
    }
}

pub fn encode_config_yaml(snapshot: &SettingsConfigSnapshot) -> String {
    let mut root = empty_mapping();
    set_value(
        &mut root,
        &CONFIG_SCHEMA_VERSION_PATH,
        Value::Number(Number::from(CURRENT_CONFIG_SCHEMA_VERSION)),
    );

    for value in &snapshot.values {
        let path = dotted_path(&value.key);
        if path.is_empty() {
            continue;
        }

        set_value(&mut root, &path, config_value_to_yaml(value));
    }

    serialize_yaml(&root)
}

pub fn load_config_snapshot(config_text: &str) -> LoadedConfig {
    let mut root = parse_root(config_text);
    let source_version = read_schema_version(&root);
    let mut had_future_version = false;
    let had_unsupported_path = false;
    let migrated_version;
    let should_write_back;

    if source_version > CURRENT_CONFIG_SCHEMA_VERSION {
        had_future_version = true;
        migrated_version = source_version;
        should_write_back = false;
    } else {
        migrated_version = CURRENT_CONFIG_SCHEMA_VERSION;
        set_value(
            &mut root,
            &CONFIG_SCHEMA_VERSION_PATH,
            Value::Number(Number::from(CURRENT_CONFIG_SCHEMA_VERSION)),
        );
        should_write_back = source_version != migrated_version;
    }

    let mut values = Vec::new();
    flatten_config_values("", &root, &mut values);

    LoadedConfig {
        values,
        source_version,
        migrated_version,
        had_future_version,
        had_unsupported_path,
        should_write_back,
        serialized_yaml: serialize_yaml(&root),
    }
}

fn empty_mapping() -> Value {
    Value::Mapping(Mapping::new())
}

fn parse_root(serialized: &str) -> Value {
    if serialized.trim().is_empty() {
        return empty_mapping();
    }

    match serde_yaml_ng::from_str::<Value>(serialized) {
        Ok(Value::Mapping(mapping)) => Value::Mapping(mapping),
        Ok(_) | Err(_) => empty_mapping(),
    }
}

fn dotted_path(key: &str) -> Vec<&str> {
    key.split('.').filter(|segment| !segment.is_empty()).collect()
}

fn value_at_path<'a>(root: &'a Value, path: &[&str]) -> Option<&'a Value> {
    let mut current = root;

    for key in path {
        current = match current {
            Value::Mapping(map) => map.get(Value::String((*key).to_owned()))?,
            _ => return None,
        };
    }

    Some(current)
}

fn read_schema_version(root: &Value) -> i32 {
    match value_at_path(root, &CONFIG_SCHEMA_VERSION_PATH) {
        Some(Value::Number(number)) => number.as_i64().unwrap_or_default().max(0) as i32,
        Some(Value::String(value)) => value.parse::<i32>().ok().unwrap_or_default().max(0),
        _ => 0,
    }
}

fn parse_scalar_f32(value: &Value) -> Option<f32> {
    match value {
        Value::Number(number) => number.as_f64().map(|value| value as f32),
        Value::String(value) => value.trim().parse::<f32>().ok(),
        _ => None,
    }
}

fn normalize_scale_factor(factor: f32) -> Option<f32> {
    (1.0..=3.0).contains(&factor).then_some(factor)
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

fn set_value(root: &mut Value, path: &[&str], value: Value) {
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

fn config_value_to_yaml(value: &SettingsConfigValue) -> Value {
    match value.kind {
        SettingsConfigValueKind::Bool => Value::Bool(value.bool_value),
        SettingsConfigValueKind::Int => Value::Number(Number::from(value.int_value)),
        SettingsConfigValueKind::Double => {
            serde_yaml_ng::to_value(value.double_value).unwrap_or(Value::Null)
        }
        SettingsConfigValueKind::String => Value::String(value.string_value.clone()),
        SettingsConfigValueKind::StringList => Value::Sequence(
            value
                .string_list_value
                .iter()
                .map(|entry| Value::String(entry.clone()))
                .collect(),
        ),
        SettingsConfigValueKind::StringListMap => string_list_map(&value.string_list_map_value),
        _ => Value::Null,
    }
}

fn flatten_config_values(prefix: &str, value: &Value, values: &mut Vec<SettingsConfigValue>) {
    match value {
        Value::Mapping(mapping) => {
            if !prefix.is_empty() {
                if let Some(entries) = mapping_as_string_list_map(mapping) {
                    values.push(SettingsConfigValue {
                        key: prefix.to_owned(),
                        kind: SettingsConfigValueKind::StringListMap,
                        bool_value: false,
                        int_value: 0,
                        double_value: 0.0,
                        string_value: String::new(),
                        string_list_value: vec![],
                        string_list_map_value: entries,
                    });
                    return;
                }
            }

            for (key, child) in mapping {
                let Value::String(key) = key else {
                    continue;
                };

                let next_prefix = if prefix.is_empty() {
                    key.clone()
                } else {
                    format!("{prefix}.{key}")
                };
                if next_prefix == "meta.settings_schema_version" {
                    continue;
                }

                flatten_config_values(&next_prefix, child, values);
            }
        }
        Value::Bool(bool_value) => values.push(SettingsConfigValue {
            key: prefix.to_owned(),
            kind: SettingsConfigValueKind::Bool,
            bool_value: *bool_value,
            int_value: 0,
            double_value: 0.0,
            string_value: String::new(),
            string_list_value: vec![],
            string_list_map_value: vec![],
        }),
        Value::Number(number) => {
            if let Some(int_value) = number.as_i64().and_then(|value| i32::try_from(value).ok()) {
                values.push(SettingsConfigValue {
                    key: prefix.to_owned(),
                    kind: SettingsConfigValueKind::Int,
                    bool_value: false,
                    int_value,
                    double_value: 0.0,
                    string_value: String::new(),
                    string_list_value: vec![],
                    string_list_map_value: vec![],
                });
            } else if let Some(double_value) = number.as_f64() {
                values.push(SettingsConfigValue {
                    key: prefix.to_owned(),
                    kind: SettingsConfigValueKind::Double,
                    bool_value: false,
                    int_value: 0,
                    double_value,
                    string_value: String::new(),
                    string_list_value: vec![],
                    string_list_map_value: vec![],
                });
            }
        }
        Value::String(string_value) => values.push(SettingsConfigValue {
            key: prefix.to_owned(),
            kind: SettingsConfigValueKind::String,
            bool_value: false,
            int_value: 0,
            double_value: 0.0,
            string_value: string_value.clone(),
            string_list_value: vec![],
            string_list_map_value: vec![],
        }),
        Value::Sequence(sequence) => {
            let mut strings = Vec::new();
            for entry in sequence {
                let Value::String(string_value) = entry else {
                    return;
                };
                strings.push(string_value.clone());
            }

            values.push(SettingsConfigValue {
                key: prefix.to_owned(),
                kind: SettingsConfigValueKind::StringList,
                bool_value: false,
                int_value: 0,
                double_value: 0.0,
                string_value: String::new(),
                string_list_value: strings,
                string_list_map_value: vec![],
            });
        }
        _ => {}
    }
}

fn mapping_as_string_list_map(mapping: &Mapping) -> Option<Vec<SettingsStringListMapEntry>> {
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

fn string_list_map(entries: &[SettingsStringListMapEntry]) -> Value {
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

fn serialize_yaml(root: &Value) -> String {
    let mut serialized = serde_yaml_ng::to_string(root).unwrap_or_default();
    if let Some(rest) = serialized.strip_prefix("---\n") {
        serialized = rest.to_owned();
    }
    serialized
}

#[cfg(test)]
mod tests {
    use super::{encode_config_yaml, parse_config_text};
    use crate::ffi::{
        SettingsConfigSnapshot, SettingsConfigValue, SettingsConfigValueKind, SettingsStringListMapEntry,
    };

    #[test]
    fn parses_valid_scale_factor() {
        let config = parse_config_text(
            r#"
ui:
  scale:
    factor: 1.75
"#,
        );

        assert_eq!(config.ui_scale_factor, Some(1.75));
    }

    #[test]
    fn ignores_out_of_range_scale_factor() {
        let config = parse_config_text(
            r#"
ui:
  scale:
    factor: 5
"#,
        );

        assert_eq!(config.ui_scale_factor, None);
    }

    #[test]
    fn ignores_malformed_scale_factor() {
        let config = parse_config_text(
            r#"
ui:
  scale:
    factor: nope
"#,
        );

        assert_eq!(config.ui_scale_factor, None);
    }

    #[test]
    fn encodes_generic_config_values() {
        let yaml = encode_config_yaml(&SettingsConfigSnapshot {
            values: vec![
                SettingsConfigValue {
                    key: "meta.ignored".to_owned(),
                    kind: SettingsConfigValueKind::String,
                    bool_value: false,
                    int_value: 0,
                    double_value: 0.0,
                    string_value: "x".to_owned(),
                    string_list_value: vec![],
                    string_list_map_value: vec![],
                },
                SettingsConfigValue {
                    key: "ui.theme.slug".to_owned(),
                    kind: SettingsConfigValueKind::String,
                    bool_value: false,
                    int_value: 0,
                    double_value: 0.0,
                    string_value: "komai-dark".to_owned(),
                    string_list_value: vec![],
                    string_list_map_value: vec![],
                },
                SettingsConfigValue {
                    key: "timeline.hidden_events.global".to_owned(),
                    kind: SettingsConfigValueKind::StringList,
                    bool_value: false,
                    int_value: 0,
                    double_value: 0.0,
                    string_value: String::new(),
                    string_list_value: vec!["m.reaction".to_owned()],
                    string_list_map_value: vec![],
                },
                SettingsConfigValue {
                    key: "timeline.hidden_events.by_room".to_owned(),
                    kind: SettingsConfigValueKind::StringListMap,
                    bool_value: false,
                    int_value: 0,
                    double_value: 0.0,
                    string_value: String::new(),
                    string_list_value: vec![],
                    string_list_map_value: vec![SettingsStringListMapEntry {
                        key: "!room:example.org".to_owned(),
                        values: vec!["m.call.candidates".to_owned()],
                    }],
                },
            ],
        });

        let root: serde_yaml_ng::Value = serde_yaml_ng::from_str(&yaml).expect("valid yaml");
        assert!(matches!(
            super::value_at_path(&root, &["meta", "settings_schema_version"]),
            Some(serde_yaml_ng::Value::Number(number)) if number.as_i64() == Some(1)
        ));
        assert!(matches!(
            super::value_at_path(&root, &["ui", "theme", "slug"]),
            Some(serde_yaml_ng::Value::String(value)) if value == "komai-dark"
        ));
        assert!(matches!(
            super::value_at_path(&root, &["timeline", "hidden_events", "global"]),
            Some(serde_yaml_ng::Value::Sequence(_))
        ));
        assert!(matches!(
            super::value_at_path(&root, &["timeline", "hidden_events", "by_room"]),
            Some(serde_yaml_ng::Value::Mapping(_))
        ));
    }
}
