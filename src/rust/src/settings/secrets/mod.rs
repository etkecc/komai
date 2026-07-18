// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::BTreeMap;

use serde_yaml_ng::{Mapping, Value};

use crate::ffi::{MatrixPersistedSessionSecrets, SettingsSecretsPayload, SettingsStringMapEntry};

use super::storage;

const INTERNAL_SESSION_ACCESS_TOKEN_KEY: &str = "__session.access_token";
const INTERNAL_SESSION_KEY_PREFIX: &str = "__session.";
const NAMED_SECRETS_ROOT_KEY: &str = "secrets";
const PROFILE_SECRETS_LABEL: &str = "secrets";
const MATRIX_SDK_SECRETS_LABEL: &str = "matrix-sdk secrets";
const PROFILE_SECURE_STORE_KEY_NAME: &str = "session.secrets";
const MATRIX_SDK_STORE_PASSPHRASE_KEY: &str = "matrix_sdk.store_passphrase";
const MATRIX_SDK_HOMESERVER_URL_KEY: &str = "matrix_sdk.homeserver_url";
const MATRIX_SDK_SERIALIZED_SESSION_KEY: &str = "matrix_sdk.serialized_session";
const MATRIX_SDK_SECURE_STORE_KEY_NAME: &str = "matrix_sdk.session";

fn ordered_map(entries: &[SettingsStringMapEntry]) -> BTreeMap<String, String> {
    entries
        .iter()
        .map(|entry| (entry.key.clone(), entry.value.clone()))
        .collect()
}

fn encode_yaml_value(value: &Value) -> String {
    let mut serialized = serde_yaml_ng::to_string(value).unwrap_or_default();
    if let Some(rest) = serialized.strip_prefix("---\n") {
        serialized = rest.to_owned();
    }
    serialized
}

fn parse_yaml_root(serialized: &str) -> Option<Value> {
    if serialized.trim().is_empty() {
        return None;
    }

    serde_yaml_ng::from_str::<Value>(serialized).ok()
}

fn decode_string_map_value(value: Option<&Value>) -> Vec<SettingsStringMapEntry> {
    let Some(Value::Mapping(mapping)) = value else {
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

fn encode_profile_secrets_entries(
    access_token: &str,
    entries: &[SettingsStringMapEntry],
) -> Vec<SettingsStringMapEntry> {
    let mut encoded = BTreeMap::new();

    for entry in entries {
        if entry.key.starts_with(INTERNAL_SESSION_KEY_PREFIX) || entry.value.is_empty() {
            continue;
        }

        encoded.insert(entry.key.clone(), entry.value.clone());
    }

    if !access_token.is_empty() {
        encoded.insert(
            INTERNAL_SESSION_ACCESS_TOKEN_KEY.to_owned(),
            access_token.to_owned(),
        );
    }

    encoded
        .into_iter()
        .map(|(key, value)| SettingsStringMapEntry { key, value })
        .collect()
}

fn decode_profile_secrets_entries(entries: &[SettingsStringMapEntry]) -> SettingsSecretsPayload {
    let mut access_token = String::new();
    let mut secrets = BTreeMap::new();
    let mut had_stale_values = false;

    for entry in entries {
        if entry.key == INTERNAL_SESSION_ACCESS_TOKEN_KEY {
            if entry.value.is_empty() {
                had_stale_values = true;
            } else {
                access_token = entry.value.clone();
            }
            continue;
        }

        if entry.key.starts_with(INTERNAL_SESSION_KEY_PREFIX) || entry.value.is_empty() {
            had_stale_values = true;
            continue;
        }

        secrets.insert(entry.key.clone(), entry.value.clone());
    }

    SettingsSecretsPayload {
        access_token,
        secrets: secrets
            .into_iter()
            .map(|(key, value)| SettingsStringMapEntry { key, value })
            .collect(),
        had_stale_values,
    }
}

pub fn encode_string_map_yaml(entries: &[SettingsStringMapEntry]) -> String {
    let value = serde_yaml_ng::to_value(ordered_map(entries)).unwrap_or(Value::Mapping(Mapping::new()));
    encode_yaml_value(&value)
}

pub fn decode_string_map_yaml(serialized: &str) -> Vec<SettingsStringMapEntry> {
    decode_string_map_value(parse_yaml_root(serialized).as_ref())
}

pub fn encode_persisted_secrets_map_yaml(
    access_token: &str,
    entries: &[SettingsStringMapEntry],
) -> String {
    encode_string_map_yaml(&encode_profile_secrets_entries(access_token, entries))
}

pub fn decode_persisted_secrets_map_yaml(serialized: &str) -> SettingsSecretsPayload {
    decode_profile_secrets_entries(&decode_string_map_yaml(serialized))
}

pub fn encode_named_string_map_yaml(root_key: &str, entries: &[SettingsStringMapEntry]) -> String {
    let mut root = Mapping::new();
    root.insert(
        Value::String(root_key.to_owned()),
        serde_yaml_ng::to_value(ordered_map(entries)).unwrap_or(Value::Mapping(Mapping::new())),
    );
    encode_yaml_value(&Value::Mapping(root))
}

pub fn decode_named_string_map_yaml(
    serialized: &str,
    root_key: &str,
) -> Vec<SettingsStringMapEntry> {
    let root = parse_yaml_root(serialized);
    let Some(Value::Mapping(mapping)) = root.as_ref() else {
        return Vec::new();
    };

    decode_string_map_value(mapping.get(Value::String(root_key.to_owned())))
}

pub fn load_persisted_secrets_file_from_path(
    path: &str,
    label: &str,
    root_key: &str,
) -> SettingsSecretsPayload {
    decode_persisted_secrets_file_yaml(&storage::read_text_file(path, label), root_key)
}

pub fn load_persisted_secrets_file_for_profile(profile_id: &str) -> SettingsSecretsPayload {
    let secrets_path = storage::secrets_file_path_for_profile(profile_id);
    load_persisted_secrets_file_from_path(
        &secrets_path,
        PROFILE_SECRETS_LABEL,
        NAMED_SECRETS_ROOT_KEY,
    )
}

pub fn write_persisted_secrets_file_to_path(
    path: &str,
    root_key: &str,
    access_token: &str,
    entries: &[SettingsStringMapEntry],
    owner_read_write_only: bool,
) -> bool {
    storage::write_text_file(
        path,
        &encode_persisted_secrets_file_yaml(root_key, access_token, entries),
        owner_read_write_only,
    )
}

pub fn write_persisted_secrets_file_for_profile(
    profile_id: &str,
    access_token: &str,
    entries: &[SettingsStringMapEntry],
    owner_read_write_only: bool,
) -> bool {
    let secrets_path = storage::secrets_file_path_for_profile(profile_id);
    write_persisted_secrets_file_to_path(
        &secrets_path,
        NAMED_SECRETS_ROOT_KEY,
        access_token,
        entries,
        owner_read_write_only,
    )
}

pub fn encode_persisted_secrets_file_yaml(
    root_key: &str,
    access_token: &str,
    entries: &[SettingsStringMapEntry],
) -> String {
    encode_named_string_map_yaml(root_key, &encode_profile_secrets_entries(access_token, entries))
}

pub fn decode_persisted_secrets_file_yaml(
    serialized: &str,
    root_key: &str,
) -> SettingsSecretsPayload {
    decode_profile_secrets_entries(&decode_named_string_map_yaml(serialized, root_key))
}

pub fn load_matrix_sdk_secrets_for_profile(profile_id: &str) -> Vec<SettingsStringMapEntry> {
    let secrets_path = storage::matrix_sdk_secrets_file_path_for_profile(profile_id);
    load_named_string_map_from_path(&secrets_path, MATRIX_SDK_SECRETS_LABEL, NAMED_SECRETS_ROOT_KEY)
}

pub fn write_matrix_sdk_secrets_for_profile(
    profile_id: &str,
    entries: &[SettingsStringMapEntry],
    owner_read_write_only: bool,
) -> bool {
    let secrets_path = storage::matrix_sdk_secrets_file_path_for_profile(profile_id);
    write_named_string_map_to_path(
        &secrets_path,
        NAMED_SECRETS_ROOT_KEY,
        entries,
        owner_read_write_only,
    )
}

pub fn remove_matrix_sdk_secrets_file_for_profile(profile_id: &str) -> bool {
    let secrets_path = storage::matrix_sdk_secrets_file_path_for_profile(profile_id);
    if !storage::path_exists(&secrets_path) {
        return true;
    }

    storage::remove_path(&secrets_path)
}

fn uses_file_secrets_provider_for_profile(profile_id: &str) -> bool {
    let config_path = storage::config_file_path_for_profile(profile_id);
    let config_text = storage::read_text_file(&config_path, "config");
    let config = crate::settings::config::parse_config_text(&config_text);
    config.secrets.provider.to_storage_string() == "file"
}

fn load_matrix_sdk_secrets_from_secure_store(profile_id: &str) -> Vec<SettingsStringMapEntry> {
    let secure_store_key = storage::secure_store_key(profile_id, MATRIX_SDK_SECURE_STORE_KEY_NAME);
    let Some(serialized) = storage::read_secure_value(&secure_store_key) else {
        return Vec::new();
    };

    if serialized.is_empty() {
        storage::delete_secure_value(&secure_store_key);
        return Vec::new();
    }

    decode_string_map_yaml(&serialized)
}

// Blocking, result-checked writes: the serialized session carries the OAuth
// refresh token, which the server rotates on every refresh. A fire-and-forget
// write that fails (or is dropped because the process exits before the queued
// keyring job runs) leaves a stale refresh token behind, and the next refresh
// attempt with it permanently invalidates the whole session (invalid_grant).
fn save_matrix_sdk_secrets_to_secure_store(
    profile_id: &str,
    entries: &[SettingsStringMapEntry],
) -> bool {
    let secure_store_key = storage::secure_store_key(profile_id, MATRIX_SDK_SECURE_STORE_KEY_NAME);
    if entries.is_empty() {
        return storage::delete_secure_value_blocking(&secure_store_key);
    }

    storage::write_secure_value_blocking(&secure_store_key, &encode_string_map_yaml(entries))
}

pub fn load_persisted_matrix_session_secrets(profile_id: &str) -> MatrixPersistedSessionSecrets {
    let entries = if uses_file_secrets_provider_for_profile(profile_id) {
        load_matrix_sdk_secrets_for_profile(profile_id)
    } else {
        load_matrix_sdk_secrets_from_secure_store(profile_id)
    };

    let mut persisted = MatrixPersistedSessionSecrets {
        store_passphrase: String::new(),
        homeserver_url: String::new(),
        serialized_session: String::new(),
    };
    for entry in entries {
        match entry.key.as_str() {
            MATRIX_SDK_STORE_PASSPHRASE_KEY => persisted.store_passphrase = entry.value,
            MATRIX_SDK_HOMESERVER_URL_KEY => persisted.homeserver_url = entry.value,
            MATRIX_SDK_SERIALIZED_SESSION_KEY => persisted.serialized_session = entry.value,
            _ => {}
        }
    }

    persisted
}

pub fn save_persisted_matrix_session_secrets(
    profile_id: &str,
    store_passphrase: &str,
    homeserver_url: &str,
    serialized_session: &str,
) -> bool {
    let mut entries = Vec::new();
    if !store_passphrase.is_empty() {
        entries.push(SettingsStringMapEntry {
            key: MATRIX_SDK_STORE_PASSPHRASE_KEY.to_owned(),
            value: store_passphrase.to_owned(),
        });
    }
    if !homeserver_url.is_empty() {
        entries.push(SettingsStringMapEntry {
            key: MATRIX_SDK_HOMESERVER_URL_KEY.to_owned(),
            value: homeserver_url.to_owned(),
        });
    }
    if !serialized_session.is_empty() {
        entries.push(SettingsStringMapEntry {
            key: MATRIX_SDK_SERIALIZED_SESSION_KEY.to_owned(),
            value: serialized_session.to_owned(),
        });
    }

    if uses_file_secrets_provider_for_profile(profile_id) {
        if entries.is_empty() {
            return remove_matrix_sdk_secrets_file_for_profile(profile_id);
        }

        return write_matrix_sdk_secrets_for_profile(profile_id, entries.as_slice(), true);
    }

    save_matrix_sdk_secrets_to_secure_store(profile_id, entries.as_slice())
}

pub fn clear_persisted_matrix_session_secrets(profile_id: &str) -> bool {
    save_persisted_matrix_session_secrets(profile_id, "", "", "")
}

pub fn load_profile_secrets(
    profile_id: &str,
    uses_file_secrets_provider: bool,
) -> SettingsSecretsPayload {
    if uses_file_secrets_provider {
        let payload = load_persisted_secrets_file_for_profile(profile_id);
        if payload.had_stale_values {
            let _ = save_profile_secrets(
                profile_id,
                true,
                &payload.access_token,
                payload.secrets.as_slice(),
            );
        }
        return payload;
    }

    let secrets_key = storage::secure_store_key(profile_id, PROFILE_SECURE_STORE_KEY_NAME);
    let Some(serialized) = storage::read_secure_value(&secrets_key) else {
        return SettingsSecretsPayload {
            access_token: String::new(),
            secrets: Vec::new(),
            had_stale_values: false,
        };
    };

    if serialized.is_empty() {
        storage::delete_secure_value(&secrets_key);
        return SettingsSecretsPayload {
            access_token: String::new(),
            secrets: Vec::new(),
            had_stale_values: true,
        };
    }

    let payload = decode_persisted_secrets_map_yaml(&serialized);
    if payload.had_stale_values {
        let _ = save_profile_secrets(
            profile_id,
            false,
            &payload.access_token,
            payload.secrets.as_slice(),
        );
    }
    payload
}

pub fn save_profile_secrets(
    profile_id: &str,
    uses_file_secrets_provider: bool,
    access_token: &str,
    entries: &[SettingsStringMapEntry],
) -> bool {
    if uses_file_secrets_provider {
        return write_persisted_secrets_file_for_profile(
            profile_id,
            access_token,
            entries,
            true,
        );
    }

    let secrets_key = storage::secure_store_key(profile_id, PROFILE_SECURE_STORE_KEY_NAME);
    let serialized = encode_persisted_secrets_map_yaml(access_token, entries);
    let has_persisted_secrets = !access_token.trim().is_empty()
        || entries
            .iter()
            .any(|entry| !entry.value.is_empty() && !entry.key.starts_with(INTERNAL_SESSION_KEY_PREFIX));

    if has_persisted_secrets {
        storage::write_secure_value(&secrets_key, &serialized);
    } else {
        storage::delete_secure_value(&secrets_key);
    }

    let secrets_path = storage::secrets_file_path_for_profile(profile_id);
    if storage::path_exists(&secrets_path) {
        let _ = storage::remove_path(&secrets_path);
    }

    true
}

pub fn clear_profile_secrets(profile_id: &str, uses_file_secrets_provider: bool) -> bool {
    if uses_file_secrets_provider {
        return remove_persisted_secrets_file_for_profile(profile_id);
    }

    let all_removed =
        crate::ffi::settings_delete_all_profile_secrets_from_store(profile_id, false);
    let secrets_path = storage::secrets_file_path_for_profile(profile_id);
    if storage::path_exists(&secrets_path) && !storage::remove_path(&secrets_path) {
        return false;
    }

    all_removed
}

pub fn load_named_string_map_from_path(
    path: &str,
    label: &str,
    root_key: &str,
) -> Vec<SettingsStringMapEntry> {
    decode_named_string_map_yaml(&storage::read_text_file(path, label), root_key)
}

pub fn write_named_string_map_to_path(
    path: &str,
    root_key: &str,
    entries: &[SettingsStringMapEntry],
    owner_read_write_only: bool,
) -> bool {
    storage::write_text_file(
        path,
        &encode_named_string_map_yaml(root_key, entries),
        owner_read_write_only,
    )
}

pub fn remove_persisted_secrets_file_for_profile(profile_id: &str) -> bool {
    let secrets_path = storage::secrets_file_path_for_profile(profile_id);
    if !storage::path_exists(&secrets_path) {
        return true;
    }

    storage::remove_path(&secrets_path)
}

#[cfg(test)]
mod tests;
