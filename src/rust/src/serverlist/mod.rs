// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod model;

use crate::ffi::{ServerListEntry, ServerListResult};
use model::{ServerEntry, ServerListFile};

const SERVERS_YAML: &str = include_str!("../../../../resources/serverlist/servers.yml");

pub fn entries() -> ServerListResult {
    match serde_yaml_ng::from_str::<ServerListFile>(SERVERS_YAML) {
        Ok(parsed) => {
            if let Err(error) = parsed.validate() {
                return ServerListResult {
                    entries: Vec::new(),
                    error_message: error,
                };
            }

            let mut entries: Vec<ServerListEntry> =
                parsed.servers.iter().map(to_ffi_entry).collect();

            entries.sort_by(|a, b| a.rank.cmp(&b.rank).then_with(|| a.name.cmp(&b.name)));

            ServerListResult {
                entries,
                error_message: String::new(),
            }
        }
        Err(error) => ServerListResult {
            entries: Vec::new(),
            error_message: format!("failed to parse server list: {error}"),
        },
    }
}

fn to_ffi_entry(entry: &ServerEntry) -> ServerListEntry {
    ServerListEntry {
        name: entry.name.clone(),
        client_domain: entry.client_domain.clone(),
        description: entry.description.clone(),
        homepage: entry.homepage.clone(),
        using_vanilla_reg: entry.using_vanilla_reg,
        languages: entry.languages.clone(),
        software: entry.software.clone(),
        staff_jur: entry.staff_jur.clone(),
        rules: entry.rules.clone(),
        privacy: entry.privacy.clone(),
        captcha: entry.captcha,
        email: entry.email,
        features: entry.features.clone(),
        sliding_sync: entry.sliding_sync,
        reg_link: entry.reg_link.clone(),
        reg_note: entry.reg_note.clone(),
        rank: entry.rank,
        category: entry.category.clone(),
        editorial: entry.editorial.clone(),
        featured: entry.featured,
    }
}
