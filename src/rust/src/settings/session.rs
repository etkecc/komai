// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Mapping, Number, Value};

const CURRENT_SESSION_SCHEMA_VERSION: i32 = 1;

const SESSION_SCHEMA_VERSION_PATH: [&str; 2] = ["meta", "settings_schema_version"];
const SESSION_USER_ID_PATH: [&str; 3] = ["session", "account", "user_id"];
const SESSION_HOMESERVER_PATH: [&str; 3] = ["session", "account", "homeserver"];
const SESSION_DEVICE_ID_PATH: [&str; 3] = ["session", "device", "id"];

pub struct LoadedSession {
    pub user_id: String,
    pub device_id: String,
    pub homeserver: String,
    pub source_version: i32,
    pub migrated_version: i32,
    pub had_future_version: bool,
    pub had_unsupported_path: bool,
    pub should_write_back: bool,
    pub serialized_yaml: String,
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

fn read_string(root: &Value, path: &[&str]) -> String {
    let mut current = root;
    for segment in path {
        let Value::Mapping(mapping) = current else {
            return String::new();
        };
        let Some(next) = mapping.get(Value::String((*segment).to_owned())) else {
            return String::new();
        };
        current = next;
    }

    match current {
        Value::String(value) => value.trim().to_owned(),
        _ => String::new(),
    }
}

fn read_schema_version(root: &Value) -> i32 {
    let mut current = root;
    for segment in SESSION_SCHEMA_VERSION_PATH {
        let Value::Mapping(mapping) = current else {
            return 0;
        };
        let Some(next) = mapping.get(Value::String(segment.to_owned())) else {
            return 0;
        };
        current = next;
    }

    match current {
        Value::Number(number) => number.as_i64().unwrap_or_default().max(0) as i32,
        Value::String(value) => value.parse::<i32>().ok().unwrap_or_default().max(0),
        _ => 0,
    }
}

fn ensure_mapping<'a>(mapping: &'a mut Mapping, key: &str) -> &'a mut Mapping {
    let key = Value::String(key.to_owned());
    let needs_init = !matches!(mapping.get(&key), Some(Value::Mapping(_)));
    if needs_init {
        mapping.insert(key.clone(), Value::Mapping(Mapping::new()));
    }

    let Some(Value::Mapping(next)) = mapping.get_mut(&key) else {
        unreachable!();
    };
    next
}

fn set_string(root: &mut Value, path: &[&str], value: &str) {
    let Value::Mapping(mapping) = root else {
        *root = Value::Mapping(Mapping::new());
        set_string(root, path, value);
        return;
    };

    let mut current = mapping;
    for segment in &path[..path.len() - 1] {
        current = ensure_mapping(current, segment);
    }

    current.insert(
        Value::String(path[path.len() - 1].to_owned()),
        Value::String(value.to_owned()),
    );
}

fn stamp_schema_version(root: &mut Value, version: i32) {
    let Value::Mapping(mapping) = root else {
        *root = Value::Mapping(Mapping::new());
        stamp_schema_version(root, version);
        return;
    };

    let meta = ensure_mapping(mapping, SESSION_SCHEMA_VERSION_PATH[0]);
    meta.insert(
        Value::String(SESSION_SCHEMA_VERSION_PATH[1].to_owned()),
        Value::Number(Number::from(version)),
    );
}

fn serialize_yaml(value: &Value) -> String {
    let mut serialized = serde_yaml_ng::to_string(value).unwrap_or_default();
    if let Some(rest) = serialized.strip_prefix("---\n") {
        serialized = rest.to_owned();
    }
    serialized
}

pub fn load_session_snapshot(session_text: &str) -> LoadedSession {
    let mut root = parse_root(session_text);
    let source_version = read_schema_version(&root);
    let mut had_future_version = false;
    let had_unsupported_path = false;
    let migrated_version;
    let should_write_back;

    if source_version > CURRENT_SESSION_SCHEMA_VERSION {
        had_future_version = true;
        migrated_version = source_version;
        should_write_back = false;
    } else {
        migrated_version = CURRENT_SESSION_SCHEMA_VERSION;
        stamp_schema_version(&mut root, CURRENT_SESSION_SCHEMA_VERSION);
        should_write_back = source_version != migrated_version;
    }

    LoadedSession {
        user_id: read_string(&root, &SESSION_USER_ID_PATH),
        device_id: read_string(&root, &SESSION_DEVICE_ID_PATH),
        homeserver: read_string(&root, &SESSION_HOMESERVER_PATH),
        source_version,
        migrated_version,
        had_future_version,
        had_unsupported_path,
        should_write_back,
        serialized_yaml: serialize_yaml(&root),
    }
}

pub fn encode_session_yaml(user_id: &str, homeserver: &str, device_id: &str) -> String {
    let mut root = empty_mapping();
    stamp_schema_version(&mut root, CURRENT_SESSION_SCHEMA_VERSION);
    set_string(&mut root, &SESSION_USER_ID_PATH, user_id);
    set_string(&mut root, &SESSION_HOMESERVER_PATH, homeserver);
    set_string(&mut root, &SESSION_DEVICE_ID_PATH, device_id);
    serialize_yaml(&root)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn session_snapshot_migrates_and_normalizes() {
        let loaded = load_session_snapshot(
            "session:\n  account:\n    user_id: \" @u:hs \"\n    homeserver: \" https://hs \"\n  device:\n    id: \" DEV \"\n",
        );

        assert_eq!(loaded.user_id, "@u:hs");
        assert_eq!(loaded.homeserver, "https://hs");
        assert_eq!(loaded.device_id, "DEV");
        assert_eq!(loaded.source_version, 0);
        assert_eq!(loaded.migrated_version, 1);
        assert!(loaded.should_write_back);
    }

    #[test]
    fn session_snapshot_respects_future_version() {
        let loaded = load_session_snapshot("meta:\n  settings_schema_version: 3\n");

        assert!(loaded.had_future_version);
        assert!(!loaded.should_write_back);
        assert_eq!(loaded.migrated_version, 3);
    }

    #[test]
    fn encoded_session_yaml_contains_schema_and_identity() {
        let encoded = encode_session_yaml("@u:hs", "https://hs", "DEV");
        let loaded = load_session_snapshot(&encoded);

        assert_eq!(loaded.user_id, "@u:hs");
        assert_eq!(loaded.homeserver, "https://hs");
        assert_eq!(loaded.device_id, "DEV");
        assert_eq!(loaded.migrated_version, 1);
    }
}
