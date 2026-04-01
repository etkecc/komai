// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Number, Value};

use crate::ffi::{
    SettingsConfigSnapshot, SettingsConfigValue, SettingsConfigValueKind, SettingsStringListMapEntry,
};

const CURRENT_CONFIG_SCHEMA_VERSION: i32 = 1;

#[derive(Clone, Debug, Default)]
pub struct StoredConfig {
    pub ui_scale_factor: Option<f32>,
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
        &["meta", "settings_schema_version"],
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

fn empty_mapping() -> Value {
    Value::Mapping(Mapping::new())
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
