// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Value};

use crate::ffi::{SettingsStateSnapshot, SettingsStringMapEntry};

use super::{storage, yaml};

const CURRENT_STATE_SCHEMA_VERSION: i32 = 1;

const DEFAULT_WINDOW_WIDTH: i32 = 1600;
const DEFAULT_WINDOW_HEIGHT: i32 = 900;
const DEFAULT_ROOM_LIST_WIDTH: i32 = 400;
const DEFAULT_COMMUNITIES_WIDTH: i32 = 220;
const DEFAULT_BADGES_HIDDEN_FILTERS: [&str; 2] = ["global", "tag:m.lowpriority"];

const STATE_SCHEMA_VERSION_PATH: [&str; 2] = ["meta", "settings_schema_version"];
const WINDOW_WIDTH_PATH: [&str; 3] = ["ui", "window", "width_px"];
const WINDOW_HEIGHT_PATH: [&str; 3] = ["ui", "window", "height_px"];
const ROOM_LIST_WIDTH_PATH: [&str; 3] = ["sidebars", "room_list", "width_px"];
const CURRENT_ROOM_ID_PATH: [&str; 3] = ["sidebars", "room_list", "current_room_id"];
const COMMUNITIES_WIDTH_PATH: [&str; 3] = ["sidebars", "communities", "width_px"];
const GLOBAL_EXCLUDES_PATH: [&str; 4] = ["sidebars", "communities", "filtering", "global_excludes"];
const BADGES_HIDDEN_PATH: [&str; 4] = ["sidebars", "communities", "filtering", "badges_hidden"];
const COLLAPSED_SPACES_PATH: [&str; 4] =
    ["sidebars", "communities", "filtering", "collapsed_spaces"];
const CURRENT_FILTER_PATH: [&str; 4] = ["sidebars", "communities", "filtering", "current"];
const HIDDEN_PINS_PATH: [&str; 3] = ["timeline", "pins", "hidden"];
const HIDDEN_WIDGETS_PATH: [&str; 3] = ["timeline", "widgets", "hidden"];
const COMPOSER_DRAFTS_PATH: [&str; 3] = ["composer", "drafts", "by_room"];

pub struct LoadedState {
    pub window_width: i32,
    pub window_height: i32,
    pub sidebars_room_list_width_px: i32,
    pub sidebars_communities_width_px: i32,
    pub current_filter_id: String,
    pub current_room_id: String,
    pub global_excludes: Vec<String>,
    pub badges_hidden_filters: Vec<String>,
    pub hidden_pins: Vec<String>,
    pub hidden_widgets: Vec<String>,
    pub collapsed_spaces: Vec<String>,
    pub composer_drafts_by_room: Vec<SettingsStringMapEntry>,
    pub source_version: i32,
    pub migrated_version: i32,
    pub had_future_version: bool,
    pub had_unsupported_path: bool,
    pub should_write_back: bool,
    pub serialized_yaml: String,
}

fn read_int(root: &Value, path: &[&str], default: i32) -> i32 {
    match yaml::value_at_path(root, path) {
        Some(Value::Number(number)) => number.as_i64().unwrap_or(default as i64) as i32,
        Some(Value::String(value)) => value.parse::<i32>().ok().unwrap_or(default),
        _ => default,
    }
}

fn read_string(root: &Value, path: &[&str]) -> String {
    match yaml::value_at_path(root, path) {
        Some(Value::String(value)) => value.clone(),
        _ => String::new(),
    }
}

fn read_string_list(root: &Value, path: &[&str], default: &[&str]) -> Vec<String> {
    match yaml::value_at_path(root, path) {
        Some(Value::Sequence(values)) => values
            .iter()
            .filter_map(|value| match value {
                Value::String(value) => Some(value.clone()),
                _ => None,
            })
            .collect(),
        _ => default.iter().map(|value| (*value).to_owned()).collect(),
    }
}

fn read_string_map(root: &Value, path: &[&str]) -> Vec<SettingsStringMapEntry> {
    let Some(Value::Mapping(mapping)) = yaml::value_at_path(root, path) else {
        return Vec::new();
    };

    let mut result = Vec::new();
    for (key, value) in mapping {
        let (Value::String(key), Value::String(value)) = (key, value) else {
            continue;
        };
        result.push(SettingsStringMapEntry {
            key: key.clone(),
            value: value.clone(),
        });
    }
    result.sort_by(|left, right| left.key.cmp(&right.key));
    result
}

fn stamp_schema_version(root: &mut Value, version: i32) {
    yaml::set_value(
        root,
        &STATE_SCHEMA_VERSION_PATH,
        yaml::number_value(version),
    );
}

pub fn load_state_snapshot(state_text: &str) -> LoadedState {
    let mut root = yaml::parse_root(state_text);
    let source_version = yaml::read_schema_version(&root, &STATE_SCHEMA_VERSION_PATH);
    let mut had_future_version = false;
    let had_unsupported_path = false;
    let migrated_version;
    let should_write_back;

    if source_version > CURRENT_STATE_SCHEMA_VERSION {
        had_future_version = true;
        migrated_version = source_version;
        should_write_back = false;
    } else {
        migrated_version = CURRENT_STATE_SCHEMA_VERSION;
        stamp_schema_version(&mut root, CURRENT_STATE_SCHEMA_VERSION);
        should_write_back = source_version != migrated_version;
    }

    LoadedState {
        window_width: read_int(&root, &WINDOW_WIDTH_PATH, DEFAULT_WINDOW_WIDTH),
        window_height: read_int(&root, &WINDOW_HEIGHT_PATH, DEFAULT_WINDOW_HEIGHT),
        sidebars_room_list_width_px: read_int(&root, &ROOM_LIST_WIDTH_PATH, DEFAULT_ROOM_LIST_WIDTH),
        sidebars_communities_width_px: read_int(
            &root,
            &COMMUNITIES_WIDTH_PATH,
            DEFAULT_COMMUNITIES_WIDTH,
        ),
        current_filter_id: read_string(&root, &CURRENT_FILTER_PATH),
        current_room_id: read_string(&root, &CURRENT_ROOM_ID_PATH),
        global_excludes: read_string_list(&root, &GLOBAL_EXCLUDES_PATH, &[]),
        badges_hidden_filters: read_string_list(
            &root,
            &BADGES_HIDDEN_PATH,
            &DEFAULT_BADGES_HIDDEN_FILTERS,
        ),
        hidden_pins: read_string_list(&root, &HIDDEN_PINS_PATH, &[]),
        hidden_widgets: read_string_list(&root, &HIDDEN_WIDGETS_PATH, &[]),
        collapsed_spaces: read_string_list(&root, &COLLAPSED_SPACES_PATH, &[]),
        composer_drafts_by_room: read_string_map(&root, &COMPOSER_DRAFTS_PATH),
        source_version,
        migrated_version,
        had_future_version,
        had_unsupported_path,
        should_write_back,
        serialized_yaml: yaml::serialize_yaml(&root),
    }
}

fn string_sequence(values: &[String]) -> Value {
    Value::Sequence(values.iter().map(|value| Value::String(value.clone())).collect())
}

fn string_map(entries: &[SettingsStringMapEntry]) -> Value {
    let mut mapping = Mapping::new();
    for entry in entries {
        mapping.insert(
            Value::String(entry.key.clone()),
            Value::String(entry.value.clone()),
        );
    }
    Value::Mapping(mapping)
}

pub fn encode_state_yaml(snapshot: &SettingsStateSnapshot) -> String {
    let mut root = yaml::empty_mapping();
    stamp_schema_version(&mut root, CURRENT_STATE_SCHEMA_VERSION);
    yaml::set_value(
        &mut root,
        &WINDOW_WIDTH_PATH,
        yaml::number_value(snapshot.window_width),
    );
    yaml::set_value(
        &mut root,
        &WINDOW_HEIGHT_PATH,
        yaml::number_value(snapshot.window_height),
    );
    yaml::set_value(
        &mut root,
        &ROOM_LIST_WIDTH_PATH,
        yaml::number_value(snapshot.sidebars_room_list_width_px),
    );
    yaml::set_value(
        &mut root,
        &COMMUNITIES_WIDTH_PATH,
        yaml::number_value(snapshot.sidebars_communities_width_px),
    );
    yaml::set_value(
        &mut root,
        &CURRENT_FILTER_PATH,
        Value::String(snapshot.current_filter_id.clone()),
    );
    yaml::set_value(
        &mut root,
        &CURRENT_ROOM_ID_PATH,
        Value::String(snapshot.current_room_id.clone()),
    );
    yaml::set_value(
        &mut root,
        &GLOBAL_EXCLUDES_PATH,
        string_sequence(&snapshot.global_excludes),
    );
    yaml::set_value(
        &mut root,
        &BADGES_HIDDEN_PATH,
        string_sequence(&snapshot.badges_hidden_filters),
    );
    yaml::set_value(
        &mut root,
        &HIDDEN_PINS_PATH,
        string_sequence(&snapshot.hidden_pins),
    );
    yaml::set_value(
        &mut root,
        &HIDDEN_WIDGETS_PATH,
        string_sequence(&snapshot.hidden_widgets),
    );
    yaml::set_value(
        &mut root,
        &COLLAPSED_SPACES_PATH,
        string_sequence(&snapshot.collapsed_spaces),
    );
    yaml::set_value(
        &mut root,
        &COMPOSER_DRAFTS_PATH,
        string_map(&snapshot.composer_drafts_by_room),
    );
    yaml::serialize_yaml(&root)
}

pub fn write_state_snapshot_to_path(state_path: &str, snapshot: &SettingsStateSnapshot) -> bool {
    storage::write_text_file(state_path, &encode_state_yaml(snapshot), false)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn entry(key: &str, value: &str) -> SettingsStringMapEntry {
        SettingsStringMapEntry {
            key: key.to_owned(),
            value: value.to_owned(),
        }
    }

    #[test]
    fn state_snapshot_loads_defaults_and_migrates() {
        let loaded = load_state_snapshot("");

        assert_eq!(loaded.window_width, DEFAULT_WINDOW_WIDTH);
        assert_eq!(loaded.window_height, DEFAULT_WINDOW_HEIGHT);
        assert_eq!(loaded.sidebars_room_list_width_px, DEFAULT_ROOM_LIST_WIDTH);
        assert_eq!(loaded.sidebars_communities_width_px, DEFAULT_COMMUNITIES_WIDTH);
        assert_eq!(
            loaded.badges_hidden_filters,
            DEFAULT_BADGES_HIDDEN_FILTERS
                .iter()
                .map(|value| (*value).to_owned())
                .collect::<Vec<_>>()
        );
        assert!(loaded.should_write_back);
    }

    #[test]
    fn state_snapshot_loads_lists_and_maps() {
        let loaded = load_state_snapshot(
            "sidebars:\n  communities:\n    filtering:\n      current: people\n      global_excludes:\n        - one\n        - two\n      badges_hidden:\n        - x\n      collapsed_spaces:\n        - !space:hs\ncomposer:\n  drafts:\n    by_room:\n      !room:hs: draft\n",
        );

        assert_eq!(loaded.current_filter_id, "people");
        assert_eq!(loaded.global_excludes, vec!["one".to_owned(), "two".to_owned()]);
        assert_eq!(loaded.badges_hidden_filters, vec!["x".to_owned()]);
        assert_eq!(loaded.collapsed_spaces, vec!["!space:hs".to_owned()]);
        assert_eq!(loaded.composer_drafts_by_room, vec![entry("!room:hs", "draft")]);
    }

    #[test]
    fn state_yaml_roundtrip() {
        let encoded = encode_state_yaml(&SettingsStateSnapshot {
            window_width: 1200,
            window_height: 800,
            sidebars_room_list_width_px: 320,
            sidebars_communities_width_px: 240,
            current_filter_id: "people".to_owned(),
            current_room_id: "!room:hs".to_owned(),
            global_excludes: vec!["global".to_owned()],
            badges_hidden_filters: vec!["x".to_owned()],
            hidden_pins: vec!["!pin".to_owned()],
            hidden_widgets: vec!["!widget".to_owned()],
            collapsed_spaces: vec!["!space:hs".to_owned()],
            composer_drafts_by_room: vec![entry("!room:hs", "draft")],
        });
        let loaded = load_state_snapshot(&encoded);

        assert_eq!(loaded.window_width, 1200);
        assert_eq!(loaded.window_height, 800);
        assert_eq!(loaded.current_filter_id, "people");
        assert_eq!(loaded.current_room_id, "!room:hs");
        assert_eq!(loaded.composer_drafts_by_room, vec![entry("!room:hs", "draft")]);
    }
}
