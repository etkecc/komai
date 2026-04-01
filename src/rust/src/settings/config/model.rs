// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::BTreeMap;

pub(crate) const CURRENT_CONFIG_SCHEMA_VERSION: i32 = 1;
pub(crate) const CONFIG_SCHEMA_VERSION_PATH: [&str; 2] = ["meta", "settings_schema_version"];

#[derive(Clone, Debug, Default)]
pub struct Config {
    pub ui: ConfigUi,
    pub timeline: ConfigTimeline,
    pub secrets: ConfigSecrets,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUi {
    pub scale: ConfigUiScale,
    pub theme: ConfigUiTheme,
    pub input: ConfigUiInput,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiScale {
    pub factor: Option<f32>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiTheme {
    pub slug: String,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiInput {
    pub mode: String,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimeline {
    pub hidden_events: ConfigTimelineHiddenEvents,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineHiddenEvents {
    pub global: Option<Vec<String>>,
    pub by_room: BTreeMap<String, Vec<String>>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigSecrets {
    pub provider: String,
}

pub struct LoadedConfig {
    pub config: Config,
    pub values: Vec<crate::ffi::SettingsConfigValue>,
    pub source_version: i32,
    pub migrated_version: i32,
    pub had_future_version: bool,
    pub had_unsupported_path: bool,
    pub should_write_back: bool,
    pub serialized_yaml: String,
}
