// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::HashMap;

use serde::Deserialize;

/// A text field that can be either a plain string or a locale map.
///
/// When a locale map, keys are locale codes (e.g. `en`, `de`, `pt_BR`)
/// matching the `QLocale::name()` convention (underscores, not dashes).
#[derive(Debug, Deserialize)]
#[serde(untagged)]
pub enum LocalizedText {
    Localized(HashMap<String, String>),
    Plain(String),
}

impl Default for LocalizedText {
    fn default() -> Self {
        LocalizedText::Plain(String::new())
    }
}

impl LocalizedText {
    /// Resolve the best text for the given locale.
    ///
    /// Fallback chain: exact match → language-only → `en` → first available → empty.
    pub fn resolve(&self, locale: &str) -> String {
        match self {
            LocalizedText::Plain(s) => s.clone(),
            LocalizedText::Localized(map) => {
                if map.is_empty() {
                    return String::new();
                }

                let normalized = locale.replace('-', "_");

                if let Some(val) = map.get(&normalized) {
                    return val.clone();
                }

                if let Some(lang) = normalized.split('_').next() {
                    if lang != normalized {
                        if let Some(val) = map.get(lang) {
                            return val.clone();
                        }
                    }
                }

                if let Some(val) = map.get("en") {
                    return val.clone();
                }

                map.values().next().cloned().unwrap_or_default()
            }
        }
    }

    pub fn is_empty(&self) -> bool {
        match self {
            LocalizedText::Plain(s) => s.trim().is_empty(),
            LocalizedText::Localized(map) => {
                map.is_empty() || map.values().all(|v| v.trim().is_empty())
            }
        }
    }
}

#[derive(Debug, Deserialize)]
pub struct ServerListFile {
    pub servers: Vec<ServerEntry>,
}

#[derive(Debug, Deserialize)]
pub struct ServerEntry {
    pub name: String,
    pub client_domain: String,
    pub description: LocalizedText,
    #[serde(default)]
    pub homepage: String,
    pub using_vanilla_reg: bool,
    pub languages: Vec<String>,
    #[serde(default)]
    pub software: String,
    #[serde(default)]
    pub staff_jur: String,
    #[serde(default)]
    pub rules: String,
    #[serde(default)]
    pub privacy: String,
    #[serde(default)]
    pub captcha: bool,
    #[serde(default)]
    pub email: bool,
    #[serde(default)]
    pub features: Vec<String>,
    #[serde(default)]
    pub sliding_sync: bool,
    #[serde(default)]
    pub reg_link: String,
    #[serde(default)]
    pub reg_note: String,
    #[serde(default = "default_rank")]
    pub rank: i32,
    #[serde(default = "default_category")]
    pub category: String,
    #[serde(default)]
    pub editorial: LocalizedText,
    #[serde(default)]
    pub featured: bool,
}

fn default_rank() -> i32 {
    500
}

fn default_category() -> String {
    "general".to_owned()
}

impl ServerListFile {
    pub fn validate(&self) -> Result<(), String> {
        if self.servers.is_empty() {
            return Err("server list is empty".to_owned());
        }
        for (index, entry) in self.servers.iter().enumerate() {
            entry
                .validate()
                .map_err(|e| format!("servers[{index}] ({}): {e}", entry.name))?;
        }
        Ok(())
    }
}

impl ServerEntry {
    pub fn validate(&self) -> Result<(), String> {
        if self.name.trim().is_empty() {
            return Err("missing or empty 'name'".to_owned());
        }
        if self.client_domain.trim().is_empty() {
            return Err("missing or empty 'client_domain'".to_owned());
        }
        if self.description.is_empty() {
            return Err("missing or empty 'description'".to_owned());
        }
        if self.languages.is_empty() {
            return Err("'languages' must have at least 1 entry".to_owned());
        }
        Ok(())
    }
}
