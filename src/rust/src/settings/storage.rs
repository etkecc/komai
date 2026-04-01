// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::ffi;

pub fn read_text_file(path: &str, label: &str) -> String {
    ffi::settings_read_text_file(path, label)
}

pub fn write_text_file(path: &str, content: &str, owner_read_write_only: bool) -> bool {
    ffi::settings_write_text_file(path, content, owner_read_write_only)
}
