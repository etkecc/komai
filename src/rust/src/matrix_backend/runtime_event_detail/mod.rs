// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

//! Extracts structured detail data from state events for enriched messages.
//!
//! Pure data extraction — no English strings. Translated to user-visible text
//! by C++ `StateEventText::translatePowerLevels()` (src/timeline/StateEventText.cpp).

use matrix_sdk::ruma::events::room::{
    power_levels::RoomPowerLevelsEventContent,
    server_acl::RoomServerAclEventContent,
};

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ServerAclChange {
    /// Servers added to the allow list.
    pub allowed_added: Vec<String>,
    /// Servers removed from the allow list.
    pub allowed_removed: Vec<String>,
    /// Servers added to the deny list.
    pub denied_added: Vec<String>,
    /// Servers removed from the deny list.
    pub denied_removed: Vec<String>,
    /// `Some(new_value)` when `allow_ip_literals` changed, `None` otherwise.
    pub ip_literals_changed: Option<bool>,
}

impl ServerAclChange {
    pub fn is_empty(&self) -> bool {
        self.allowed_added.is_empty()
            && self.allowed_removed.is_empty()
            && self.denied_added.is_empty()
            && self.denied_removed.is_empty()
            && self.ip_literals_changed.is_none()
    }

    pub fn total_changes(&self) -> usize {
        self.allowed_added.len()
            + self.allowed_removed.len()
            + self.denied_added.len()
            + self.denied_removed.len()
            + usize::from(self.ip_literals_changed.is_some())
    }
}

/// Diff a server-ACL event against its predecessor.
///
/// Translated to user-visible text in C++ `StateEventText::translateServerAcl()`.
pub fn diff_server_acl(
    content: &RoomServerAclEventContent,
    prev_content: &RoomServerAclEventContent,
) -> ServerAclChange {
    let allowed_set: std::collections::BTreeSet<_> = content.allow.iter().collect();
    let prev_allowed_set: std::collections::BTreeSet<_> = prev_content.allow.iter().collect();

    let denied_set: std::collections::BTreeSet<_> = content.deny.iter().collect();
    let prev_denied_set: std::collections::BTreeSet<_> = prev_content.deny.iter().collect();

    ServerAclChange {
        allowed_added: allowed_set
            .difference(&prev_allowed_set)
            .map(|s| (*s).clone())
            .collect(),
        allowed_removed: prev_allowed_set
            .difference(&allowed_set)
            .map(|s| (*s).clone())
            .collect(),
        denied_added: denied_set
            .difference(&prev_denied_set)
            .map(|s| (*s).clone())
            .collect(),
        denied_removed: prev_denied_set
            .difference(&denied_set)
            .map(|s| (*s).clone())
            .collect(),
        ip_literals_changed: if content.allow_ip_literals != prev_content.allow_ip_literals {
            Some(content.allow_ip_literals)
        } else {
            None
        },
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct PowerLevelChange {
    pub user_id: String,
    pub old_level: i64,
    pub new_level: i64,
}

/// Diff the `users` maps of a power-levels event against its predecessor.
///
/// Returns only users whose effective level actually changed.  For users not
/// in a map, the respective `users_default` is used as their level.
///
/// Translated to user-visible text in C++ `StateEventText::translatePowerLevels()`.
pub fn diff_power_level_users(
    content: &RoomPowerLevelsEventContent,
    prev_content: &RoomPowerLevelsEventContent,
) -> Vec<PowerLevelChange> {
    let mut changes = Vec::new();

    // Collect the union of user IDs from both maps.
    let mut all_users: std::collections::BTreeSet<_> = content.users.keys().collect();
    all_users.extend(prev_content.users.keys());

    let new_default: i64 = content.users_default.into();
    let old_default: i64 = prev_content.users_default.into();

    for user_id in all_users {
        let old_level: i64 = prev_content
            .users
            .get(user_id)
            .map(|level| i64::from(*level))
            .unwrap_or(old_default);

        let new_level: i64 = content
            .users
            .get(user_id)
            .map(|level| i64::from(*level))
            .unwrap_or(new_default);

        if old_level != new_level {
            changes.push(PowerLevelChange {
                user_id: user_id.to_string(),
                old_level,
                new_level,
            });
        }
    }

    changes
}

#[cfg(test)]
mod tests;
