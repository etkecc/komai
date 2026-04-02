// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod bridge;
mod model;
mod tree;

use crate::ffi::SettingsConfigSnapshot;
use crate::settings::yaml;

use super::storage;

pub use model::{
    Config, ConfigCalls, ConfigCallsAudio, ConfigCallsDevices, ConfigCallsLegacy,
    ConfigCallsRelay, ConfigCallsScreenshare, ConfigComposer, ConfigEncryption,
    ConfigEncryptionBackup, ConfigEncryptionBackupOnline, ConfigEncryptionKeySharing,
    ConfigSecrets, ConfigTimeline, ConfigTimelineHiddenEvents, ConfigUi, ConfigUiAvatars,
    ConfigIntegrations, ConfigNetwork, ConfigNotifications, ConfigPrivacy,
    ConfigPrivacyMaintenance, ConfigPrivacyWindowFocusBlur, ConfigSidebars,
    ConfigSidebarsCommunities, ConfigSidebarsRoomList, ConfigUiFont, ConfigUiInput,
    ConfigUiLayout, ConfigUiMotion, ConfigUiScale, ConfigUiTheme, LoadedConfig,
};

const UI_SCALE_FACTOR_PATH: [&str; 3] = ["ui", "scale", "factor"];
const UI_THEME_SLUG_PATH: [&str; 3] = ["ui", "theme", "slug"];
const UI_FONT_FAMILY_PATH: [&str; 3] = ["ui", "font", "family"];
const UI_FONT_EMOJI_FAMILY_PATH: [&str; 3] = ["ui", "font", "emoji_family"];
const UI_FONT_SIZE_PT_PATH: [&str; 3] = ["ui", "font", "size_pt"];
const UI_MOTION_ANIMATIONS_ENABLED_PATH: [&str; 3] = ["ui", "motion", "enable_animations"];
const UI_INPUT_MODE_PATH: [&str; 3] = ["ui", "input", "mode"];
const UI_INPUT_TOUCH_SWIPE_GESTURES_ENABLED_PATH: [&str; 5] =
    ["ui", "input", "touch", "swipe_gestures", "enabled"];
const UI_LAYOUT_CONTENT_MAX_WIDTH_PX_PATH: [&str; 4] = ["ui", "layout", "content", "max_width_px"];
const UI_LAYOUT_COMPACT_MODE_PATH: [&str; 3] = ["ui", "layout", "compact_mode"];
const UI_AVATARS_CIRCULAR_PATH: [&str; 3] = ["ui", "avatars", "circular"];
const UI_AVATARS_DEFAULT_AVATAR_STYLE_PATH: [&str; 3] = ["ui", "avatars", "default_avatar_style"];
const UI_SCROLLBAR_POLICY_PATH: [&str; 2] = ["ui", "scrollbar_policy"];
const SIDEBARS_ROOM_LIST_SHOW_LAST_MESSAGE_TIME_PATH: [&str; 3] =
    ["sidebars", "room_list", "show_last_message_timestamp"];
const SIDEBARS_ROOM_LIST_LAST_MESSAGE_PREVIEW_PATH: [&str; 3] =
    ["sidebars", "room_list", "last_message_preview"];
const SIDEBARS_ROOM_LIST_SHOW_COMMUNITY_COUNTS_PATH: [&str; 3] = [
    "sidebars",
    "room_list",
    "show_community_notification_counts",
];
const SIDEBARS_ROOM_LIST_SORT_PATH: [&str; 3] = ["sidebars", "room_list", "sort"];
const SIDEBARS_ROOM_LIST_UNREAD_DETECTION_POLICY_PATH: [&str; 3] =
    ["sidebars", "room_list", "unread_detection_policy"];
const SIDEBARS_COMMUNITIES_VISIBLE_PATH: [&str; 3] = ["sidebars", "communities", "visible"];
const SIDEBARS_COMMUNITIES_FILTER_FAVOURITES_PATH: [&str; 4] =
    ["sidebars", "communities", "filters", "favourites"];
const SIDEBARS_COMMUNITIES_FILTER_PEOPLE_PATH: [&str; 4] =
    ["sidebars", "communities", "filters", "people"];
const SIDEBARS_COMMUNITIES_FILTER_BOTS_PATH: [&str; 4] =
    ["sidebars", "communities", "filters", "bots"];
const SIDEBARS_COMMUNITIES_FILTER_GROUPS_PATH: [&str; 4] =
    ["sidebars", "communities", "filters", "groups"];
const SIDEBARS_COMMUNITIES_FILTER_SERVER_NOTICES_PATH: [&str; 4] =
    ["sidebars", "communities", "filters", "server_notices"];
const SIDEBARS_COMMUNITIES_FILTER_LOW_PRIORITY_PATH: [&str; 4] =
    ["sidebars", "communities", "filters", "low_priority"];
const HIDDEN_EVENTS_GLOBAL_PATH: [&str; 3] = ["timeline", "hidden_events", "global"];
const HIDDEN_EVENTS_BY_ROOM_PATH: [&str; 3] = ["timeline", "hidden_events", "by_room"];
const SECRETS_PROVIDER_PATH: [&str; 2] = ["secrets", "provider"];
const PRIVACY_WINDOW_FOCUS_BLUR_ENABLED_PATH: [&str; 3] =
    ["privacy", "window_focus_blur", "enabled"];
const PRIVACY_WINDOW_FOCUS_BLUR_DELAY_SECONDS_PATH: [&str; 3] =
    ["privacy", "window_focus_blur", "delay_seconds"];
const PRIVACY_MAINTENANCE_EXPIRE_EVENTS_PATH: [&str; 3] =
    ["privacy", "maintenance", "expire_events"];
const ENCRYPTION_KEY_SHARING_ONLY_VERIFIED_USERS_PATH: [&str; 3] =
    ["encryption", "key_sharing", "only_verified_users"];
const ENCRYPTION_KEY_SHARING_SHARE_WITH_TRUSTED_PATH: [&str; 3] =
    ["encryption", "key_sharing", "share_with_trusted"];
const ENCRYPTION_BACKUP_ONLINE_ENABLED_PATH: [&str; 4] =
    ["encryption", "backup", "online", "enabled"];
const CALLS_LEGACY_ENABLED_PATH: [&str; 3] = ["calls", "legacy", "enabled"];
const CALLS_RELAY_USE_FALLBACK_SERVER_PATH: [&str; 3] =
    ["calls", "relay", "use_fallback_server"];
const CALLS_DEVICES_MICROPHONE_PATH: [&str; 3] = ["calls", "devices", "microphone"];
const CALLS_DEVICES_CAMERA_PATH: [&str; 3] = ["calls", "devices", "camera"];
const CALLS_DEVICES_CAMERA_RESOLUTION_PATH: [&str; 3] =
    ["calls", "devices", "camera_resolution"];
const CALLS_DEVICES_CAMERA_FRAME_RATE_PATH: [&str; 3] =
    ["calls", "devices", "camera_frame_rate"];
const CALLS_AUDIO_RINGTONE_PATH: [&str; 3] = ["calls", "audio", "ringtone"];
const CALLS_SCREENSHARE_FRAME_RATE_PATH: [&str; 3] = ["calls", "screenshare", "frame_rate"];
const CALLS_SCREENSHARE_PICTURE_IN_PICTURE_PATH: [&str; 3] =
    ["calls", "screenshare", "picture_in_picture"];
const CALLS_SCREENSHARE_INCLUDE_REMOTE_VIDEO_PATH: [&str; 3] =
    ["calls", "screenshare", "include_remote_video"];
const CALLS_SCREENSHARE_SHOW_CURSOR_PATH: [&str; 3] =
    ["calls", "screenshare", "show_cursor"];
const NOTIFICATIONS_ENABLED_PATH: [&str; 2] = ["notifications", "enabled"];
const NOTIFICATIONS_ATTENTION_ON_INCOMING_PATH: [&str; 2] =
    ["notifications", "attention_on_incoming"];
const NOTIFICATIONS_MESSAGE_CONTENT_POLICY_PATH: [&str; 2] =
    ["notifications", "message_content_policy"];
const NETWORK_PRESENCE_STATUS_POLICY_PATH: [&str; 3] = ["network", "presence", "status_policy"];
const NETWORK_TLS_ENABLE_CERTIFICATE_VALIDATION_PATH: [&str; 3] =
    ["network", "tls", "enable_certificate_validation"];
const NETWORK_MRS_ENABLED_PATH: [&str; 2] = ["network", "mrs_enabled"];
const NETWORK_MRS_SERVER_NAME_PATH: [&str; 2] = ["network", "mrs_server_name"];
const NETWORK_HTTP3_ENABLED_PATH: [&str; 2] = ["network", "http3_enabled"];
const INTEGRATIONS_SYSTEM_TRAY_ENABLED_PATH: [&str; 3] =
    ["integrations", "system_tray", "enabled"];
const INTEGRATIONS_SYSTEM_TRAY_AUTOSTART_PATH: [&str; 3] =
    ["integrations", "system_tray", "autostart"];
const INTEGRATIONS_DBUS_API_ACCESS_PATH: [&str; 3] = ["integrations", "dbus", "access"];
const INTEGRATIONS_BROWSER_COMMAND_PATH: [&str; 3] = ["integrations", "browser", "command"];
const COMPOSER_INPUT_MARKDOWN_TO_HTML_ENABLED_PATH: [&str; 4] =
    ["composer", "input", "markdown_to_html", "enabled"];
const COMPOSER_INPUT_SEND_KEY_PATH: [&str; 3] = ["composer", "input", "send_key"];
const COMPOSER_INPUT_AUTO_REPLACE_EMOJI_PATH: [&str; 3] =
    ["composer", "input", "auto_replace_emoji"];
const COMPOSER_INPUT_EMOJI_PREFERRED_GENDER_PATH: [&str; 4] =
    ["composer", "input", "emoji", "preferred_gender"];
const COMPOSER_INPUT_EMOJI_PREFERRED_SKIN_TONE_PATH: [&str; 4] =
    ["composer", "input", "emoji", "preferred_skin_tone"];
const COMPOSER_INPUT_INLINE_EMOJI_PICKER_ENABLED_PATH: [&str; 4] =
    ["composer", "input", "inline_emoji_picker", "enabled"];
const COMPOSER_INPUT_INLINE_ROOM_PICKER_ENABLED_PATH: [&str; 4] =
    ["composer", "input", "inline_room_picker", "enabled"];
const COMPOSER_INPUT_INLINE_USER_PICKER_ENABLED_PATH: [&str; 4] =
    ["composer", "input", "inline_user_picker", "enabled"];
const COMPOSER_TYPING_SEND_ENABLED_PATH: [&str; 4] =
    ["composer", "typing", "send", "enabled"];
const COMPOSER_EXTRAS_STICKERS_ENABLED_PATH: [&str; 4] =
    ["composer", "extras", "stickers", "enabled"];

pub fn parse_config_text(config_text: &str) -> Config {
    let root = yaml::parse_root(config_text);
    parse_config_root(&root)
}

pub(crate) fn parse_config_root(root: &serde_yaml_ng::Value) -> Config {
    Config {
        ui: ConfigUi {
            scale: ConfigUiScale {
                factor: yaml::value_at_path(root, &UI_SCALE_FACTOR_PATH)
                    .and_then(parse_scalar_f32)
                    .and_then(normalize_scale_factor),
            },
            theme: ConfigUiTheme {
                slug: parse_string(yaml::value_at_path(root, &UI_THEME_SLUG_PATH)),
            },
            font: ConfigUiFont {
                family: parse_string(yaml::value_at_path(root, &UI_FONT_FAMILY_PATH)),
                emoji_family: parse_string(yaml::value_at_path(root, &UI_FONT_EMOJI_FAMILY_PATH)),
                size_pt: yaml::value_at_path(root, &UI_FONT_SIZE_PT_PATH)
                    .and_then(parse_scalar_f64),
            },
            motion: ConfigUiMotion {
                animations_enabled: yaml::value_at_path(root, &UI_MOTION_ANIMATIONS_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
            },
            input: ConfigUiInput {
                mode: parse_string(yaml::value_at_path(root, &UI_INPUT_MODE_PATH)),
                touch_swipe_gestures_enabled: yaml::value_at_path(
                    root,
                    &UI_INPUT_TOUCH_SWIPE_GESTURES_ENABLED_PATH,
                )
                .and_then(parse_scalar_bool),
            },
            layout: ConfigUiLayout {
                content_max_width_px: yaml::value_at_path(root, &UI_LAYOUT_CONTENT_MAX_WIDTH_PX_PATH)
                    .and_then(parse_scalar_i32),
                compact_mode: yaml::value_at_path(root, &UI_LAYOUT_COMPACT_MODE_PATH)
                    .and_then(parse_scalar_bool),
            },
            avatars: ConfigUiAvatars {
                circular: yaml::value_at_path(root, &UI_AVATARS_CIRCULAR_PATH)
                    .and_then(parse_scalar_bool),
                default_avatar_style: parse_string(
                    yaml::value_at_path(root, &UI_AVATARS_DEFAULT_AVATAR_STYLE_PATH),
                ),
            },
            scrollbar_policy: parse_string(yaml::value_at_path(root, &UI_SCROLLBAR_POLICY_PATH)),
        },
        sidebars: ConfigSidebars {
            room_list: ConfigSidebarsRoomList {
                show_last_message_time: yaml::value_at_path(
                    root,
                    &SIDEBARS_ROOM_LIST_SHOW_LAST_MESSAGE_TIME_PATH,
                )
                .and_then(parse_scalar_bool),
                last_message_preview: parse_string(yaml::value_at_path(
                    root,
                    &SIDEBARS_ROOM_LIST_LAST_MESSAGE_PREVIEW_PATH,
                )),
                show_community_counts: yaml::value_at_path(
                    root,
                    &SIDEBARS_ROOM_LIST_SHOW_COMMUNITY_COUNTS_PATH,
                )
                .and_then(parse_scalar_bool),
                sort: parse_string(yaml::value_at_path(root, &SIDEBARS_ROOM_LIST_SORT_PATH)),
                unread_detection_policy: parse_string(yaml::value_at_path(
                    root,
                    &SIDEBARS_ROOM_LIST_UNREAD_DETECTION_POLICY_PATH,
                )),
            },
            communities: ConfigSidebarsCommunities {
                visible: yaml::value_at_path(root, &SIDEBARS_COMMUNITIES_VISIBLE_PATH)
                    .and_then(parse_scalar_bool),
                filter_favourites: yaml::value_at_path(
                    root,
                    &SIDEBARS_COMMUNITIES_FILTER_FAVOURITES_PATH,
                )
                .and_then(parse_scalar_bool),
                filter_people: yaml::value_at_path(root, &SIDEBARS_COMMUNITIES_FILTER_PEOPLE_PATH)
                    .and_then(parse_scalar_bool),
                filter_bots: yaml::value_at_path(root, &SIDEBARS_COMMUNITIES_FILTER_BOTS_PATH)
                    .and_then(parse_scalar_bool),
                filter_groups: yaml::value_at_path(root, &SIDEBARS_COMMUNITIES_FILTER_GROUPS_PATH)
                    .and_then(parse_scalar_bool),
                filter_server_notices: yaml::value_at_path(
                    root,
                    &SIDEBARS_COMMUNITIES_FILTER_SERVER_NOTICES_PATH,
                )
                .and_then(parse_scalar_bool),
                filter_low_priority: yaml::value_at_path(
                    root,
                    &SIDEBARS_COMMUNITIES_FILTER_LOW_PRIORITY_PATH,
                )
                .and_then(parse_scalar_bool),
            },
        },
        timeline: ConfigTimeline {
            hidden_events: ConfigTimelineHiddenEvents {
                global: parse_string_list(yaml::value_at_path(root, &HIDDEN_EVENTS_GLOBAL_PATH)),
                by_room: parse_string_list_map(yaml::value_at_path(root, &HIDDEN_EVENTS_BY_ROOM_PATH)),
            },
        },
        secrets: ConfigSecrets {
            provider: parse_string(yaml::value_at_path(root, &SECRETS_PROVIDER_PATH)),
        },
        privacy: ConfigPrivacy {
            window_focus_blur: ConfigPrivacyWindowFocusBlur {
                enabled: yaml::value_at_path(root, &PRIVACY_WINDOW_FOCUS_BLUR_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
                delay_seconds: yaml::value_at_path(
                    root,
                    &PRIVACY_WINDOW_FOCUS_BLUR_DELAY_SECONDS_PATH,
                )
                .and_then(parse_scalar_i32),
            },
            maintenance: ConfigPrivacyMaintenance {
                expire_events: yaml::value_at_path(root, &PRIVACY_MAINTENANCE_EXPIRE_EVENTS_PATH)
                    .and_then(parse_scalar_bool),
            },
        },
        encryption: ConfigEncryption {
            key_sharing: ConfigEncryptionKeySharing {
                only_verified_users: yaml::value_at_path(
                    root,
                    &ENCRYPTION_KEY_SHARING_ONLY_VERIFIED_USERS_PATH,
                )
                .and_then(parse_scalar_bool),
                share_with_trusted: yaml::value_at_path(
                    root,
                    &ENCRYPTION_KEY_SHARING_SHARE_WITH_TRUSTED_PATH,
                )
                .and_then(parse_scalar_bool),
            },
            backup: ConfigEncryptionBackup {
                online: ConfigEncryptionBackupOnline {
                    enabled: yaml::value_at_path(root, &ENCRYPTION_BACKUP_ONLINE_ENABLED_PATH)
                        .and_then(parse_scalar_bool),
                },
            },
        },
        calls: ConfigCalls {
            legacy: ConfigCallsLegacy {
                enabled: yaml::value_at_path(root, &CALLS_LEGACY_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
            },
            relay: ConfigCallsRelay {
                use_fallback_server: yaml::value_at_path(
                    root,
                    &CALLS_RELAY_USE_FALLBACK_SERVER_PATH,
                )
                .and_then(parse_scalar_bool),
            },
            devices: ConfigCallsDevices {
                microphone: parse_string(yaml::value_at_path(root, &CALLS_DEVICES_MICROPHONE_PATH)),
                camera: parse_string(yaml::value_at_path(root, &CALLS_DEVICES_CAMERA_PATH)),
                camera_resolution: parse_string(
                    yaml::value_at_path(root, &CALLS_DEVICES_CAMERA_RESOLUTION_PATH),
                ),
                camera_frame_rate: parse_string(
                    yaml::value_at_path(root, &CALLS_DEVICES_CAMERA_FRAME_RATE_PATH),
                ),
            },
            audio: ConfigCallsAudio {
                ringtone: parse_string(yaml::value_at_path(root, &CALLS_AUDIO_RINGTONE_PATH)),
            },
            screenshare: ConfigCallsScreenshare {
                frame_rate: yaml::value_at_path(root, &CALLS_SCREENSHARE_FRAME_RATE_PATH)
                    .and_then(parse_scalar_i32),
                picture_in_picture: yaml::value_at_path(
                    root,
                    &CALLS_SCREENSHARE_PICTURE_IN_PICTURE_PATH,
                )
                .and_then(parse_scalar_bool),
                include_remote_video: yaml::value_at_path(
                    root,
                    &CALLS_SCREENSHARE_INCLUDE_REMOTE_VIDEO_PATH,
                )
                .and_then(parse_scalar_bool),
                show_cursor: yaml::value_at_path(root, &CALLS_SCREENSHARE_SHOW_CURSOR_PATH)
                    .and_then(parse_scalar_bool),
            },
        },
        notifications: ConfigNotifications {
            enabled: yaml::value_at_path(root, &NOTIFICATIONS_ENABLED_PATH).and_then(parse_scalar_bool),
            attention_on_incoming: yaml::value_at_path(root, &NOTIFICATIONS_ATTENTION_ON_INCOMING_PATH)
                .and_then(parse_scalar_bool),
            message_content_policy: parse_string(yaml::value_at_path(
                root,
                &NOTIFICATIONS_MESSAGE_CONTENT_POLICY_PATH,
            )),
        },
        network: ConfigNetwork {
            presence_status_policy: parse_string(yaml::value_at_path(
                root,
                &NETWORK_PRESENCE_STATUS_POLICY_PATH,
            )),
            tls_enable_certificate_validation: yaml::value_at_path(
                root,
                &NETWORK_TLS_ENABLE_CERTIFICATE_VALIDATION_PATH,
            )
            .and_then(parse_scalar_bool),
            mrs_enabled: yaml::value_at_path(root, &NETWORK_MRS_ENABLED_PATH)
                .and_then(parse_scalar_bool),
            mrs_server_name: parse_string(yaml::value_at_path(root, &NETWORK_MRS_SERVER_NAME_PATH)),
            http3_enabled: yaml::value_at_path(root, &NETWORK_HTTP3_ENABLED_PATH)
                .and_then(parse_scalar_bool),
        },
        integrations: ConfigIntegrations {
            system_tray_enabled: yaml::value_at_path(root, &INTEGRATIONS_SYSTEM_TRAY_ENABLED_PATH)
                .and_then(parse_scalar_bool),
            system_tray_autostart: yaml::value_at_path(
                root,
                &INTEGRATIONS_SYSTEM_TRAY_AUTOSTART_PATH,
            )
            .and_then(parse_scalar_bool),
            dbus_api_access: parse_string(yaml::value_at_path(root, &INTEGRATIONS_DBUS_API_ACCESS_PATH)),
            browser_command: parse_string(yaml::value_at_path(root, &INTEGRATIONS_BROWSER_COMMAND_PATH)),
        },
        composer: ConfigComposer {
            input_markdown_to_html_enabled: yaml::value_at_path(
                root,
                &COMPOSER_INPUT_MARKDOWN_TO_HTML_ENABLED_PATH,
            )
            .and_then(parse_scalar_bool),
            input_send_key: parse_string(yaml::value_at_path(root, &COMPOSER_INPUT_SEND_KEY_PATH)),
            input_auto_replace_emoji: parse_string(yaml::value_at_path(
                root,
                &COMPOSER_INPUT_AUTO_REPLACE_EMOJI_PATH,
            )),
            input_emoji_preferred_gender: parse_string(yaml::value_at_path(
                root,
                &COMPOSER_INPUT_EMOJI_PREFERRED_GENDER_PATH,
            )),
            input_emoji_preferred_skin_tone: parse_string(yaml::value_at_path(
                root,
                &COMPOSER_INPUT_EMOJI_PREFERRED_SKIN_TONE_PATH,
            )),
            input_inline_emoji_picker_enabled: yaml::value_at_path(
                root,
                &COMPOSER_INPUT_INLINE_EMOJI_PICKER_ENABLED_PATH,
            )
            .and_then(parse_scalar_bool),
            input_inline_room_picker_enabled: yaml::value_at_path(
                root,
                &COMPOSER_INPUT_INLINE_ROOM_PICKER_ENABLED_PATH,
            )
            .and_then(parse_scalar_bool),
            input_inline_user_picker_enabled: yaml::value_at_path(
                root,
                &COMPOSER_INPUT_INLINE_USER_PICKER_ENABLED_PATH,
            )
            .and_then(parse_scalar_bool),
            typing_send_enabled: yaml::value_at_path(root, &COMPOSER_TYPING_SEND_ENABLED_PATH)
                .and_then(parse_scalar_bool),
            extras_stickers_enabled: yaml::value_at_path(
                root,
                &COMPOSER_EXTRAS_STICKERS_ENABLED_PATH,
            )
            .and_then(parse_scalar_bool),
        },
    }
}

pub fn encode_config_yaml(snapshot: &SettingsConfigSnapshot) -> String {
    bridge::encode_config_yaml(snapshot)
}

pub fn write_config_snapshot_to_path(config_path: &str, snapshot: &SettingsConfigSnapshot) -> bool {
    storage::write_text_file(config_path, &encode_config_yaml(snapshot), false)
}

pub fn load_config_snapshot(config_text: &str) -> LoadedConfig {
    bridge::load_config_snapshot(config_text)
}

fn parse_scalar_f32(value: &serde_yaml_ng::Value) -> Option<f32> {
    match value {
        serde_yaml_ng::Value::Number(number) => number.as_f64().map(|value| value as f32),
        serde_yaml_ng::Value::String(value) => value.trim().parse::<f32>().ok(),
        _ => None,
    }
}

fn parse_scalar_f64(value: &serde_yaml_ng::Value) -> Option<f64> {
    match value {
        serde_yaml_ng::Value::Number(number) => number.as_f64(),
        serde_yaml_ng::Value::String(value) => value.trim().parse::<f64>().ok(),
        _ => None,
    }
}

fn parse_scalar_i32(value: &serde_yaml_ng::Value) -> Option<i32> {
    match value {
        serde_yaml_ng::Value::Number(number) => number.as_i64().and_then(|value| i32::try_from(value).ok()),
        serde_yaml_ng::Value::String(value) => value.trim().parse::<i32>().ok(),
        _ => None,
    }
}

fn parse_scalar_bool(value: &serde_yaml_ng::Value) -> Option<bool> {
    match value {
        serde_yaml_ng::Value::Bool(value) => Some(*value),
        serde_yaml_ng::Value::Number(number) => match number.as_i64() {
            Some(0) => Some(false),
            Some(1) => Some(true),
            _ => None,
        },
        serde_yaml_ng::Value::String(value) => match value.trim().to_ascii_lowercase().as_str() {
            "true" | "yes" | "on" | "1" => Some(true),
            "false" | "no" | "off" | "0" => Some(false),
            _ => None,
        },
        _ => None,
    }
}

fn normalize_scale_factor(factor: f32) -> Option<f32> {
    (1.0..=3.0).contains(&factor).then_some(factor)
}

fn parse_string(value: Option<&serde_yaml_ng::Value>) -> String {
    match value {
        Some(serde_yaml_ng::Value::String(value)) => value.trim().to_owned(),
        _ => String::new(),
    }
}

fn parse_string_list(value: Option<&serde_yaml_ng::Value>) -> Option<Vec<String>> {
    match value {
        Some(serde_yaml_ng::Value::Sequence(values)) => Some(
            values
                .iter()
                .filter_map(|value| match value {
                    serde_yaml_ng::Value::String(value) => Some(value.clone()),
                    _ => None,
                })
                .collect(),
        ),
        _ => None,
    }
}

fn parse_string_list_map(
    value: Option<&serde_yaml_ng::Value>,
) -> std::collections::BTreeMap<String, Vec<String>> {
    let Some(serde_yaml_ng::Value::Mapping(mapping)) = value else {
        return std::collections::BTreeMap::new();
    };

    let mut result = std::collections::BTreeMap::new();
    for (key, value) in mapping {
        let serde_yaml_ng::Value::String(key) = key else {
            continue;
        };
        let serde_yaml_ng::Value::Sequence(values) = value else {
            continue;
        };

        let mut parsed = Vec::new();
        let mut valid = true;
        for value in values {
            let serde_yaml_ng::Value::String(value) = value else {
                valid = false;
                break;
            };
            parsed.push(value.clone());
        }
        if valid {
            result.insert(key.clone(), parsed);
        }
    }

    result
}

#[cfg(test)]
mod tests;
