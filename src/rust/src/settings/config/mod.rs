// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod bridge;
mod model;
mod tree;

use crate::ffi::SettingsConfigSnapshot;
use crate::settings::yaml;

use super::storage;

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

pub fn write_config_snapshot_to_path(config_path: &str, snapshot: &SettingsConfigSnapshot) -> bool {
    storage::write_text_file(config_path, &encode_config_yaml(snapshot), false)
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
mod tests;
