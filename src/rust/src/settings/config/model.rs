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
    pub privacy: ConfigPrivacy,
    pub calls: ConfigCalls,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUi {
    pub scale: ConfigUiScale,
    pub theme: ConfigUiTheme,
    pub font: ConfigUiFont,
    pub motion: ConfigUiMotion,
    pub input: ConfigUiInput,
    pub layout: ConfigUiLayout,
    pub avatars: ConfigUiAvatars,
    pub scrollbar_policy: String,
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
pub struct ConfigUiFont {
    pub family: String,
    pub emoji_family: String,
    pub size_pt: Option<f64>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiMotion {
    pub animations_enabled: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiInput {
    pub mode: String,
    pub touch_swipe_gestures_enabled: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiLayout {
    pub content_max_width_px: Option<i32>,
    pub compact_mode: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiAvatars {
    pub circular: Option<bool>,
    pub default_avatar_style: String,
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

#[derive(Clone, Debug, Default)]
pub struct ConfigPrivacy {
    pub window_focus_blur: ConfigPrivacyWindowFocusBlur,
    pub maintenance: ConfigPrivacyMaintenance,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigPrivacyWindowFocusBlur {
    pub enabled: Option<bool>,
    pub delay_seconds: Option<i32>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigPrivacyMaintenance {
    pub expire_events: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigCalls {
    pub legacy: ConfigCallsLegacy,
    pub relay: ConfigCallsRelay,
    pub devices: ConfigCallsDevices,
    pub audio: ConfigCallsAudio,
    pub screenshare: ConfigCallsScreenshare,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigCallsLegacy {
    pub enabled: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigCallsRelay {
    pub use_fallback_server: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigCallsDevices {
    pub microphone: String,
    pub camera: String,
    pub camera_resolution: String,
    pub camera_frame_rate: String,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigCallsAudio {
    pub ringtone: String,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigCallsScreenshare {
    pub frame_rate: Option<i32>,
    pub picture_in_picture: Option<bool>,
    pub include_remote_video: Option<bool>,
    pub show_cursor: Option<bool>,
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
