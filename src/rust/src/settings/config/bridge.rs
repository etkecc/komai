// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Number, Value};

use crate::ffi::{
    SettingsConfigSnapshot, SettingsConfigValue, SettingsConfigValueKind,
};
use crate::settings::yaml;

use super::model::{CONFIG_SCHEMA_VERSION_PATH, CURRENT_CONFIG_SCHEMA_VERSION, LoadedConfig};
use super::tree;

pub(super) fn encode_config_yaml(snapshot: &SettingsConfigSnapshot) -> String {
    let mut root = yaml::empty_mapping();
    yaml::set_value(
        &mut root,
        &CONFIG_SCHEMA_VERSION_PATH,
        yaml::number_value(CURRENT_CONFIG_SCHEMA_VERSION),
    );

    for value in &snapshot.values {
        let path = tree::dotted_path(&value.key);
        if path.is_empty() {
            continue;
        }

        yaml::set_value(&mut root, &path, config_value_to_yaml(value));
    }

    if snapshot.ui.has_scale_factor {
        yaml::set_value(
            &mut root,
            &["ui", "scale", "factor"],
            serde_yaml_ng::to_value(snapshot.ui.scale_factor).unwrap_or(Value::Null),
        );
    }
    yaml::set_value(
        &mut root,
        &["ui", "theme", "slug"],
        Value::String(snapshot.ui.theme_slug.clone()),
    );
    if snapshot.ui.has_font_size_pt {
        yaml::set_value(
            &mut root,
            &["ui", "font", "size_pt"],
            serde_yaml_ng::to_value(snapshot.ui.font_size_pt).unwrap_or(Value::Null),
        );
    }
    yaml::set_value(
        &mut root,
        &["ui", "font", "family"],
        Value::String(snapshot.ui.font_family.clone()),
    );
    yaml::set_value(
        &mut root,
        &["ui", "font", "emoji_family"],
        Value::String(snapshot.ui.font_emoji_family.clone()),
    );
    if snapshot.ui.has_motion_animations_enabled {
        yaml::set_value(
            &mut root,
            &["ui", "motion", "enable_animations"],
            Value::Bool(snapshot.ui.motion_animations_enabled),
        );
    }
    yaml::set_value(
        &mut root,
        &["ui", "input", "mode"],
        Value::String(snapshot.ui.input_mode.clone()),
    );
    if snapshot.ui.has_input_touch_swipe_gestures_enabled {
        yaml::set_value(
            &mut root,
            &["ui", "input", "touch", "swipe_gestures", "enabled"],
            Value::Bool(snapshot.ui.input_touch_swipe_gestures_enabled),
        );
    }
    if snapshot.ui.has_layout_content_max_width_px {
        yaml::set_value(
            &mut root,
            &["ui", "layout", "content", "max_width_px"],
            Value::Number(Number::from(snapshot.ui.layout_content_max_width_px)),
        );
    }
    if snapshot.ui.has_layout_compact_mode {
        yaml::set_value(
            &mut root,
            &["ui", "layout", "compact_mode"],
            Value::Bool(snapshot.ui.layout_compact_mode),
        );
    }
    if snapshot.ui.has_avatars_circular {
        yaml::set_value(
            &mut root,
            &["ui", "avatars", "circular"],
            Value::Bool(snapshot.ui.avatars_circular),
        );
    }
    yaml::set_value(
        &mut root,
        &["ui", "avatars", "default_avatar_style"],
        Value::String(snapshot.ui.default_avatar_style.clone()),
    );
    yaml::set_value(
        &mut root,
        &["ui", "scrollbar_policy"],
        Value::String(snapshot.ui.scrollbar_policy.clone()),
    );

    if snapshot.timeline.hidden_events.has_global {
        yaml::set_value(
            &mut root,
            &["timeline", "hidden_events", "global"],
            Value::Sequence(
                snapshot
                    .timeline
                    .hidden_events
                    .global
                    .iter()
                    .map(|entry| Value::String(entry.clone()))
                    .collect(),
            ),
        );
    }
    yaml::set_value(
        &mut root,
        &["timeline", "hidden_events", "by_room"],
        tree::string_list_map(&snapshot.timeline.hidden_events.by_room),
    );
    yaml::set_value(
        &mut root,
        &["secrets", "provider"],
        Value::String(snapshot.secrets.provider.clone()),
    );

    yaml::serialize_yaml(&root)
}

pub(super) fn load_config_snapshot(config_text: &str) -> LoadedConfig {
    let mut root = yaml::parse_root(config_text);
    let source_version = yaml::read_schema_version(&root, &CONFIG_SCHEMA_VERSION_PATH);
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
        yaml::set_value(
            &mut root,
            &CONFIG_SCHEMA_VERSION_PATH,
            yaml::number_value(CURRENT_CONFIG_SCHEMA_VERSION),
        );
        should_write_back = source_version != migrated_version;
    }

    let mut values = Vec::new();
    flatten_config_values("", &root, &mut values);
    let config = super::parse_config_root(&root);

    LoadedConfig {
        config,
        values,
        source_version,
        migrated_version,
        had_future_version,
        had_unsupported_path,
        should_write_back,
        serialized_yaml: yaml::serialize_yaml(&root),
    }
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
        SettingsConfigValueKind::StringListMap => tree::string_list_map(&value.string_list_map_value),
        _ => Value::Null,
    }
}

fn flatten_config_values(prefix: &str, value: &Value, values: &mut Vec<SettingsConfigValue>) {
    match value {
        Value::Mapping(mapping) => {
            if !prefix.is_empty() {
                if let Some(entries) = tree::mapping_as_string_list_map(mapping) {
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
