// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod bridge;
mod model;
mod tree;

use crate::ffi::SettingsConfigSnapshot;
use crate::settings::yaml;

pub use model::{
    Config, ConfigSecrets, ConfigTimeline, ConfigTimelineHiddenEvents, ConfigUi, ConfigUiInput,
    ConfigUiScale, ConfigUiTheme, LoadedConfig,
};

const UI_SCALE_FACTOR_PATH: [&str; 3] = ["ui", "scale", "factor"];
const UI_THEME_SLUG_PATH: [&str; 3] = ["ui", "theme", "slug"];
const UI_INPUT_MODE_PATH: [&str; 3] = ["ui", "input", "mode"];
const HIDDEN_EVENTS_GLOBAL_PATH: [&str; 3] = ["timeline", "hidden_events", "global"];
const HIDDEN_EVENTS_BY_ROOM_PATH: [&str; 3] = ["timeline", "hidden_events", "by_room"];
const SECRETS_PROVIDER_PATH: [&str; 2] = ["secrets", "provider"];

pub fn parse_config_text(config_text: &str) -> Config {
    let root = yaml::parse_root(config_text);
    parse_config_root(&root)
}

pub(crate) fn parse_config_root(root: &serde_yaml_ng::Value) -> Config {
    Config {
        ui: ConfigUi {
            scale: ConfigUiScale {
                factor: yaml::value_at_path(root, &UI_SCALE_FACTOR_PATH)
                    .and_then(parse_scalar_f32)
                    .and_then(normalize_scale_factor),
            },
            theme: ConfigUiTheme {
                slug: parse_string(yaml::value_at_path(root, &UI_THEME_SLUG_PATH)),
            },
            input: ConfigUiInput {
                mode: parse_string(yaml::value_at_path(root, &UI_INPUT_MODE_PATH)),
            },
        },
        timeline: ConfigTimeline {
            hidden_events: ConfigTimelineHiddenEvents {
                global: parse_string_list(yaml::value_at_path(root, &HIDDEN_EVENTS_GLOBAL_PATH)),
                by_room: parse_string_list_map(yaml::value_at_path(root, &HIDDEN_EVENTS_BY_ROOM_PATH)),
            },
        },
        secrets: ConfigSecrets {
            provider: parse_string(yaml::value_at_path(root, &SECRETS_PROVIDER_PATH)),
        },
    }
}

pub fn encode_config_yaml(snapshot: &SettingsConfigSnapshot) -> String {
    bridge::encode_config_yaml(snapshot)
}

pub fn load_config_snapshot(config_text: &str) -> LoadedConfig {
    bridge::load_config_snapshot(config_text)
}

fn parse_scalar_f32(value: &serde_yaml_ng::Value) -> Option<f32> {
    match value {
        serde_yaml_ng::Value::Number(number) => number.as_f64().map(|value| value as f32),
        serde_yaml_ng::Value::String(value) => value.trim().parse::<f32>().ok(),
        _ => None,
    }
}

fn normalize_scale_factor(factor: f32) -> Option<f32> {
    (1.0..=3.0).contains(&factor).then_some(factor)
}

fn parse_string(value: Option<&serde_yaml_ng::Value>) -> String {
    match value {
        Some(serde_yaml_ng::Value::String(value)) => value.trim().to_owned(),
        _ => String::new(),
    }
}

fn parse_string_list(value: Option<&serde_yaml_ng::Value>) -> Option<Vec<String>> {
    match value {
        Some(serde_yaml_ng::Value::Sequence(values)) => Some(
            values
                .iter()
                .filter_map(|value| match value {
                    serde_yaml_ng::Value::String(value) => Some(value.clone()),
                    _ => None,
                })
                .collect(),
        ),
        _ => None,
    }
}

fn parse_string_list_map(
    value: Option<&serde_yaml_ng::Value>,
) -> std::collections::BTreeMap<String, Vec<String>> {
    let Some(serde_yaml_ng::Value::Mapping(mapping)) = value else {
        return std::collections::BTreeMap::new();
    };

    let mut result = std::collections::BTreeMap::new();
    for (key, value) in mapping {
        let serde_yaml_ng::Value::String(key) = key else {
            continue;
        };
        let serde_yaml_ng::Value::Sequence(values) = value else {
            continue;
        };

        let mut parsed = Vec::new();
        let mut valid = true;
        for value in values {
            let serde_yaml_ng::Value::String(value) = value else {
                valid = false;
                break;
            };
            parsed.push(value.clone());
        }
        if valid {
            result.insert(key.clone(), parsed);
        }
    }

    result
}

#[cfg(test)]
mod tests {
    use super::{encode_config_yaml, load_config_snapshot, parse_config_text};
    use crate::ffi::{
        SettingsConfigSecretsSection, SettingsConfigSnapshot, SettingsConfigTimelineHiddenEventsSection,
        SettingsConfigTimelineSection, SettingsConfigUiSection, SettingsConfigValue,
        SettingsConfigValueKind, SettingsStringListMapEntry,
    };
    use crate::settings::yaml;

    #[test]
    fn parses_valid_scale_factor() {
        let config = parse_config_text(
            r#"
ui:
  scale:
    factor: 1.75
"#,
        );

        assert_eq!(config.ui.scale.factor, Some(1.75));
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

        assert_eq!(config.ui.scale.factor, None);
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

        assert_eq!(config.ui.scale.factor, None);
    }

    #[test]
    fn parses_theme_and_secrets_provider() {
        let config = parse_config_text(
            r#"
ui:
  theme:
    slug: komai-dark
secrets:
  provider: file
"#,
        );

        assert_eq!(config.ui.theme.slug, "komai-dark");
        assert_eq!(config.ui.input.mode, "");
        assert_eq!(config.secrets.provider, "file");
    }

    #[test]
    fn parses_input_mode_and_hidden_events() {
        let config = parse_config_text(
            r#"
ui:
  input:
    mode: touch
timeline:
  hidden_events:
    global: []
    by_room:
      "!room:example.org":
        - m.call.candidates
        - m.reaction
"#,
        );

        assert_eq!(config.ui.input.mode, "touch");
        assert_eq!(config.timeline.hidden_events.global, Some(Vec::new()));
        assert_eq!(
            config.timeline.hidden_events.by_room.get("!room:example.org"),
            Some(&vec!["m.call.candidates".to_owned(), "m.reaction".to_owned()])
        );
    }

    #[test]
    fn encodes_generic_config_values() {
        let yaml = encode_config_yaml(&SettingsConfigSnapshot {
            ui: SettingsConfigUiSection {
                has_scale_factor: false,
                scale_factor: 0.0,
                theme_slug: "komai-dark".to_owned(),
                input_mode: "desktop".to_owned(),
            },
            timeline: SettingsConfigTimelineSection {
                hidden_events: SettingsConfigTimelineHiddenEventsSection {
                    has_global: true,
                    global: vec!["m.reaction".to_owned()],
                    by_room: vec![SettingsStringListMapEntry {
                        key: "!room:example.org".to_owned(),
                        values: vec!["m.call.candidates".to_owned()],
                    }],
                },
            },
            secrets: SettingsConfigSecretsSection {
                provider: "file".to_owned(),
            },
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
            ],
        });

        let root: serde_yaml_ng::Value = serde_yaml_ng::from_str(&yaml).expect("valid yaml");
        assert!(matches!(
            yaml::value_at_path(&root, &["meta", "settings_schema_version"]),
            Some(serde_yaml_ng::Value::Number(number)) if number.as_i64() == Some(1)
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "theme", "slug"]),
            Some(serde_yaml_ng::Value::String(value)) if value == "komai-dark"
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["timeline", "hidden_events", "global"]),
            Some(serde_yaml_ng::Value::Sequence(_))
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["timeline", "hidden_events", "by_room"]),
            Some(serde_yaml_ng::Value::Mapping(_))
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "input", "mode"]),
            Some(serde_yaml_ng::Value::String(value)) if value == "desktop"
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["secrets", "provider"]),
            Some(serde_yaml_ng::Value::String(value)) if value == "file"
        ));
    }

    #[test]
    fn loaded_snapshot_keeps_typed_overview_fields() {
        let loaded = load_config_snapshot(
            r#"
meta:
  settings_schema_version: 0
ui:
  scale:
    factor: 1.5
  theme:
    slug: komai-dark
secrets:
  provider: file
"#,
        );

        assert_eq!(loaded.config.ui.scale.factor, Some(1.5));
        assert_eq!(loaded.config.ui.theme.slug, "komai-dark");
        assert_eq!(loaded.config.ui.input.mode, "");
        assert_eq!(loaded.config.secrets.provider, "file");
        assert!(loaded.should_write_back);
    }

    #[test]
    fn loaded_snapshot_keeps_future_version_untouched() {
        let loaded = load_config_snapshot(
            r#"
meta:
  settings_schema_version: 8
ui:
  theme:
    slug: komai-dark
"#,
        );

        assert!(loaded.had_future_version);
        assert!(!loaded.should_write_back);
        assert_eq!(loaded.source_version, 8);
        assert_eq!(loaded.migrated_version, 8);
        assert_eq!(loaded.config.ui.theme.slug, "komai-dark");
    }

    #[test]
    fn loaded_snapshot_clamps_negative_schema_version() {
        let loaded = load_config_snapshot(
            r#"
meta:
  settings_schema_version: -5
ui:
  theme:
    slug: komai-dark
"#,
        );

        assert!(!loaded.had_future_version);
        assert_eq!(loaded.source_version, 0);
        assert_eq!(loaded.migrated_version, 1);
        assert!(loaded.should_write_back);
        assert_eq!(loaded.config.ui.theme.slug, "komai-dark");
    }

    #[test]
    fn loaded_snapshot_normalizes_non_map_root() {
        let loaded = load_config_snapshot("\"not-a-map\"");

        assert_eq!(loaded.source_version, 0);
        assert_eq!(loaded.migrated_version, 1);
        assert!(loaded.should_write_back);
        assert!(loaded.values.is_empty());
        assert_eq!(loaded.config.ui.theme.slug, "");
    }
}
