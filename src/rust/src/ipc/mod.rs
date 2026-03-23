// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pub mod client;
pub mod protocol;

pub(crate) fn normalized_profile(profile: &str) -> &str {
    if profile.is_empty() || profile == "default" {
        "default"
    } else {
        profile
    }
}

pub(crate) fn socket_name(profile: &str) -> String {
    format!("komai-cli-{}", normalized_profile(profile))
}

#[cfg(unix)]
pub mod unix;

#[cfg(any(windows, test))]
pub mod windows;
