// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi;

pub fn config_file_path_for_profile(profile_id: &str) -> String {
    ffi::settings_profile_config_path(profile_id)
}

pub fn state_file_path_for_profile(profile_id: &str) -> String {
    ffi::settings_profile_state_path(profile_id)
}

pub fn session_file_path_for_profile(profile_id: &str) -> String {
    ffi::settings_profile_session_path(profile_id)
}

pub fn secrets_file_path_for_profile(profile_id: &str) -> String {
    ffi::settings_profile_secrets_path(profile_id)
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
