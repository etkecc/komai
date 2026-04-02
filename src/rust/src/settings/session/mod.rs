// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::Value;

use super::{storage, yaml};

pub(crate) const CURRENT_SESSION_SCHEMA_VERSION: i32 = 1;

const SESSION_SCHEMA_VERSION_PATH: [&str; 2] = ["meta", "settings_schema_version"];
const SESSION_USER_ID_PATH: [&str; 3] = ["session", "account", "user_id"];
const SESSION_HOMESERVER_PATH: [&str; 3] = ["session", "account", "homeserver"];
const SESSION_DEVICE_ID_PATH: [&str; 3] = ["session", "device", "id"];

pub struct LoadedSession {
    pub user_id: String,
    pub device_id: String,
    pub homeserver: String,
    pub source_exists: bool,
    pub source_version: i32,
    pub migrated_version: i32,
    pub had_future_version: bool,
    pub had_unsupported_path: bool,
    pub should_write_back: bool,
    pub serialized_yaml: String,
}

fn read_string(root: &Value, path: &[&str]) -> String {
    match yaml::value_at_path(root, path) {
        Some(Value::String(value)) => value.trim().to_owned(),
        _ => String::new(),
    }
}

fn set_string(root: &mut Value, path: &[&str], value: &str) {
    yaml::set_value(
        root,
        path,
        Value::String(value.to_owned()),
    );
}

fn stamp_schema_version(root: &mut Value, version: i32) {
    yaml::set_value(
        root,
        &SESSION_SCHEMA_VERSION_PATH,
        yaml::number_value(version),
    );
}

pub fn load_session_snapshot(session_text: &str) -> LoadedSession {
    let mut root = yaml::parse_root(session_text);
    let source_version = yaml::read_schema_version(&root, &SESSION_SCHEMA_VERSION_PATH);
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
        source_exists: !session_text.is_empty(),
        source_version,
        migrated_version,
        had_future_version,
        had_unsupported_path,
        should_write_back,
        serialized_yaml: yaml::serialize_yaml(&root),
    }
}

pub fn encode_session_yaml(user_id: &str, homeserver: &str, device_id: &str) -> String {
    let mut root = yaml::empty_mapping();
    stamp_schema_version(&mut root, CURRENT_SESSION_SCHEMA_VERSION);
    set_string(&mut root, &SESSION_USER_ID_PATH, user_id);
    set_string(&mut root, &SESSION_HOMESERVER_PATH, homeserver);
    set_string(&mut root, &SESSION_DEVICE_ID_PATH, device_id);
    yaml::serialize_yaml(&root)
}

pub fn write_session_snapshot_to_path(
    session_path: &str,
    user_id: &str,
    homeserver: &str,
    device_id: &str,
) -> bool {
    storage::write_text_file(
        session_path,
        &encode_session_yaml(user_id, homeserver, device_id),
        false,
    )
}

#[cfg(test)]
mod tests;
