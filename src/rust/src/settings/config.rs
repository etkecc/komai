// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod bridge;
mod model;
mod tree;

use crate::ffi::SettingsConfigSnapshot;
use crate::settings::yaml;

pub use model::{
    Config, ConfigSecrets, ConfigTimeline, ConfigTimelineHiddenEvents, ConfigUi, ConfigUiAvatars,
    ConfigUiFont, ConfigUiInput, ConfigUiLayout, ConfigUiMotion, ConfigUiScale, ConfigUiTheme,
    LoadedConfig,
};

const UI_SCALE_FACTOR_PATH: [&str; 3] = ["ui", "scale", "factor"];
const UI_THEME_SLUG_PATH: [&str; 3] = ["ui", "theme", "slug"];
const UI_FONT_FAMILY_PATH: [&str; 3] = ["ui", "font", "family"];
const UI_FONT_EMOJI_FAMILY_PATH: [&str; 3] = ["ui", "font", "emoji_family"];
const UI_FONT_SIZE_PT_PATH: [&str; 3] = ["ui", "font", "size_pt"];
const UI_MOTION_ANIMATIONS_ENABLED_PATH: [&str; 3] = ["ui", "motion", "enable_animations"];
const UI_INPUT_MODE_PATH: [&str; 3] = ["ui", "input", "mode"];
const UI_INPUT_TOUCH_SWIPE_GESTURES_ENABLED_PATH: [&str; 5] =
    ["ui", "input", "touch", "swipe_gestures", "enabled"];
const UI_LAYOUT_CONTENT_MAX_WIDTH_PX_PATH: [&str; 4] = ["ui", "layout", "content", "max_width_px"];
const UI_LAYOUT_COMPACT_MODE_PATH: [&str; 3] = ["ui", "layout", "compact_mode"];
const UI_AVATARS_CIRCULAR_PATH: [&str; 3] = ["ui", "avatars", "circular"];
const UI_AVATARS_DEFAULT_AVATAR_STYLE_PATH: [&str; 3] = ["ui", "avatars", "default_avatar_style"];
const UI_SCROLLBAR_POLICY_PATH: [&str; 2] = ["ui", "scrollbar_policy"];
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
            font: ConfigUiFont {
                family: parse_string(yaml::value_at_path(root, &UI_FONT_FAMILY_PATH)),
                emoji_family: parse_string(yaml::value_at_path(root, &UI_FONT_EMOJI_FAMILY_PATH)),
                size_pt: yaml::value_at_path(root, &UI_FONT_SIZE_PT_PATH)
                    .and_then(parse_scalar_f64),
            },
            motion: ConfigUiMotion {
                animations_enabled: yaml::value_at_path(root, &UI_MOTION_ANIMATIONS_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
            },
            input: ConfigUiInput {
                mode: parse_string(yaml::value_at_path(root, &UI_INPUT_MODE_PATH)),
                touch_swipe_gestures_enabled: yaml::value_at_path(
                    root,
                    &UI_INPUT_TOUCH_SWIPE_GESTURES_ENABLED_PATH,
                )
                .and_then(parse_scalar_bool),
            },
            layout: ConfigUiLayout {
                content_max_width_px: yaml::value_at_path(root, &UI_LAYOUT_CONTENT_MAX_WIDTH_PX_PATH)
                    .and_then(parse_scalar_i32),
                compact_mode: yaml::value_at_path(root, &UI_LAYOUT_COMPACT_MODE_PATH)
                    .and_then(parse_scalar_bool),
            },
            avatars: ConfigUiAvatars {
                circular: yaml::value_at_path(root, &UI_AVATARS_CIRCULAR_PATH)
                    .and_then(parse_scalar_bool),
                default_avatar_style: parse_string(
                    yaml::value_at_path(root, &UI_AVATARS_DEFAULT_AVATAR_STYLE_PATH),
                ),
            },
            scrollbar_policy: parse_string(yaml::value_at_path(root, &UI_SCROLLBAR_POLICY_PATH)),
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

fn parse_scalar_f64(value: &serde_yaml_ng::Value) -> Option<f64> {
    match value {
        serde_yaml_ng::Value::Number(number) => number.as_f64(),
        serde_yaml_ng::Value::String(value) => value.trim().parse::<f64>().ok(),
        _ => None,
    }
}

fn parse_scalar_i32(value: &serde_yaml_ng::Value) -> Option<i32> {
    match value {
        serde_yaml_ng::Value::Number(number) => number.as_i64().and_then(|value| i32::try_from(value).ok()),
        serde_yaml_ng::Value::String(value) => value.trim().parse::<i32>().ok(),
        _ => None,
    }
}

fn parse_scalar_bool(value: &serde_yaml_ng::Value) -> Option<bool> {
    match value {
        serde_yaml_ng::Value::Bool(value) => Some(*value),
        serde_yaml_ng::Value::Number(number) => match number.as_i64() {
            Some(0) => Some(false),
            Some(1) => Some(true),
            _ => None,
        },
        serde_yaml_ng::Value::String(value) => match value.trim().to_ascii_lowercase().as_str() {
            "true" | "yes" | "on" | "1" => Some(true),
            "false" | "no" | "off" | "0" => Some(false),
            _ => None,
        },
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
    fn parses_extended_ui_section() {
        let config = parse_config_text(
            r#"
ui:
  font:
    family: Iosevka
    emoji_family: Noto Color Emoji
    size_pt: 14
  motion:
    enable_animations: true
  input:
    touch:
      swipe_gestures:
        enabled: true
  layout:
    content:
      max_width_px: 1024
    compact_mode: false
  avatars:
    circular: true
"#,
        );

        assert_eq!(config.ui.font.family, "Iosevka");
        assert_eq!(config.ui.font.emoji_family, "Noto Color Emoji");
        assert_eq!(config.ui.font.size_pt, Some(14.0));
        assert_eq!(config.ui.motion.animations_enabled, Some(true));
        assert_eq!(config.ui.input.touch_swipe_gestures_enabled, Some(true));
        assert_eq!(config.ui.layout.content_max_width_px, Some(1024));
        assert_eq!(config.ui.layout.compact_mode, Some(false));
        assert_eq!(config.ui.avatars.circular, Some(true));
        assert_eq!(config.ui.avatars.default_avatar_style, "");
        assert_eq!(config.ui.scrollbar_policy, "");
    }

    #[test]
    fn encodes_generic_config_values() {
        let yaml = encode_config_yaml(&SettingsConfigSnapshot {
            ui: SettingsConfigUiSection {
                has_scale_factor: false,
                scale_factor: 0.0,
                theme_slug: "komai-dark".to_owned(),
                has_font_size_pt: true,
                font_size_pt: 14.0,
                font_family: "Iosevka".to_owned(),
                font_emoji_family: "Noto Color Emoji".to_owned(),
                has_motion_animations_enabled: true,
                motion_animations_enabled: true,
                input_mode: "desktop".to_owned(),
                has_input_touch_swipe_gestures_enabled: true,
                input_touch_swipe_gestures_enabled: true,
                has_layout_content_max_width_px: true,
                layout_content_max_width_px: 1024,
                has_layout_compact_mode: true,
                layout_compact_mode: false,
                has_avatars_circular: true,
                avatars_circular: true,
                scrollbar_policy: "when_needed".to_owned(),
                default_avatar_style: "boring_avatars_bauhaus".to_owned(),
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
            yaml::value_at_path(&root, &["ui", "font", "family"]),
            Some(serde_yaml_ng::Value::String(value)) if value == "Iosevka"
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "font", "emoji_family"]),
            Some(serde_yaml_ng::Value::String(value)) if value == "Noto Color Emoji"
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "font", "size_pt"]),
            Some(serde_yaml_ng::Value::Number(number)) if number.as_f64() == Some(14.0)
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "motion", "enable_animations"]),
            Some(serde_yaml_ng::Value::Bool(true))
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "input", "touch", "swipe_gestures", "enabled"]),
            Some(serde_yaml_ng::Value::Bool(true))
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "layout", "content", "max_width_px"]),
            Some(serde_yaml_ng::Value::Number(number)) if number.as_i64() == Some(1024)
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "layout", "compact_mode"]),
            Some(serde_yaml_ng::Value::Bool(false))
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "avatars", "circular"]),
            Some(serde_yaml_ng::Value::Bool(true))
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "avatars", "default_avatar_style"]),
            Some(serde_yaml_ng::Value::String(value)) if value == "boring_avatars_bauhaus"
        ));
        assert!(matches!(
            yaml::value_at_path(&root, &["ui", "scrollbar_policy"]),
            Some(serde_yaml_ng::Value::String(value)) if value == "when_needed"
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
        assert_eq!(loaded.config.ui.motion.animations_enabled, None);
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
