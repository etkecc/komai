// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi;
use std::path::PathBuf;

pub fn profile_dir_path_for_profile(profile_id: &str) -> String {
    ffi::settings_profile_directory(profile_id)
}

fn profile_file_path(profile_id: &str, file_name: &str) -> String {
    PathBuf::from(profile_dir_path_for_profile(profile_id))
        .join(file_name)
        .to_string_lossy()
        .into_owned()
}

pub fn config_file_path_for_profile(profile_id: &str) -> String {
    profile_file_path(profile_id, "config.yml")
}

pub fn state_file_path_for_profile(profile_id: &str) -> String {
    profile_file_path(profile_id, "state.yml")
}

pub fn session_file_path_for_profile(profile_id: &str) -> String {
    profile_file_path(profile_id, "session.yml")
}

pub fn secrets_file_path_for_profile(profile_id: &str) -> String {
    profile_file_path(profile_id, "secrets.yml")
}

pub fn matrix_sdk_secrets_file_path_for_profile(profile_id: &str) -> String {
    profile_file_path(profile_id, "matrix-sdk-secrets.yml")
}

pub fn secure_store_key(profile_id: &str, key_name: &str) -> String {
    ffi::settings_secure_store_key(profile_id, key_name)
}

pub fn read_secure_value(key: &str) -> Option<String> {
    let value = ffi::settings_read_secure_value(key);
    value.has_value.then_some(value.value)
}

pub fn write_secure_value(key: &str, value: &str) {
    ffi::settings_write_secure_value(key, value)
}

/// Waits for the secure-store write to complete and reports whether it
/// succeeded. Use for secrets that must be durable before the caller can
/// proceed (e.g. rotated OAuth refresh tokens, where losing the write
/// invalidates the whole session).
pub fn write_secure_value_blocking(key: &str, value: &str) -> bool {
    ffi::settings_write_secure_value_blocking(key, value)
}

pub fn delete_secure_value(key: &str) {
    ffi::settings_delete_secure_value(key)
}

pub fn delete_secure_value_blocking(key: &str) -> bool {
    ffi::settings_delete_secure_value_blocking(key)
}

pub fn read_text_file(path: &str, label: &str) -> String {
    ffi::settings_read_text_file(path, label)
}

pub fn path_exists(path: &str) -> bool {
    ffi::settings_path_exists(path)
}

pub fn remove_path(path: &str) -> bool {
    ffi::settings_remove_path(path)
}

pub fn write_text_file(path: &str, content: &str, owner_read_write_only: bool) -> bool {
    ffi::settings_write_text_file(path, content, owner_read_write_only)
}
