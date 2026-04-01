// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{encode_config_yaml, load_config_snapshot, parse_config_text};
use crate::ffi::{
    SettingsConfigIntegrationsSection, SettingsConfigNetworkSection,
    SettingsConfigNotificationsSection,
    SettingsConfigSecretsSection, SettingsConfigSnapshot,
    SettingsConfigTimelineHiddenEventsSection, SettingsConfigTimelineSection,
    SettingsConfigUiSection, SettingsConfigValue, SettingsConfigValueKind,
    SettingsStringListMapEntry,
};
use crate::settings::yaml;

#[test]
fn parses_valid_scale_factor() {
    let config = parse_config_text(
        r#"
ui:
  scale:
    factor: 1.75
"#,
    );

    assert_eq!(config.ui.scale.factor, Some(1.75));
}

#[test]
fn ignores_out_of_range_scale_factor() {
    let config = parse_config_text(
        r#"
ui:
  scale:
    factor: 5
"#,
    );

    assert_eq!(config.ui.scale.factor, None);
}

#[test]
fn ignores_malformed_scale_factor() {
    let config = parse_config_text(
        r#"
ui:
  scale:
    factor: nope
"#,
    );

    assert_eq!(config.ui.scale.factor, None);
}

#[test]
fn parses_theme_and_secrets_provider() {
    let config = parse_config_text(
        r#"
ui:
  theme:
    slug: komai-dark
secrets:
  provider: file
"#,
    );

    assert_eq!(config.ui.theme.slug, "komai-dark");
    assert_eq!(config.ui.input.mode, "");
    assert_eq!(config.secrets.provider, "file");
}

#[test]
fn parses_input_mode_and_hidden_events() {
    let config = parse_config_text(
        r#"
ui:
  input:
    mode: touch
timeline:
  hidden_events:
    global: []
    by_room:
      "!room:example.org":
        - m.call.candidates
        - m.reaction
"#,
    );

    assert_eq!(config.ui.input.mode, "touch");
    assert_eq!(config.timeline.hidden_events.global, Some(Vec::new()));
    assert_eq!(
        config.timeline.hidden_events.by_room.get("!room:example.org"),
        Some(&vec!["m.call.candidates".to_owned(), "m.reaction".to_owned()])
    );
}

#[test]
fn parses_extended_ui_section() {
    let config = parse_config_text(
        r#"
ui:
  font:
    family: Iosevka
    emoji_family: Noto Color Emoji
    size_pt: 14
  motion:
    enable_animations: true
  input:
    touch:
      swipe_gestures:
        enabled: true
  layout:
    content:
      max_width_px: 1024
    compact_mode: false
  avatars:
    circular: true
"#,
    );

    assert_eq!(config.ui.font.family, "Iosevka");
    assert_eq!(config.ui.font.emoji_family, "Noto Color Emoji");
    assert_eq!(config.ui.font.size_pt, Some(14.0));
    assert_eq!(config.ui.motion.animations_enabled, Some(true));
    assert_eq!(config.ui.input.touch_swipe_gestures_enabled, Some(true));
    assert_eq!(config.ui.layout.content_max_width_px, Some(1024));
    assert_eq!(config.ui.layout.compact_mode, Some(false));
    assert_eq!(config.ui.avatars.circular, Some(true));
    assert_eq!(config.ui.avatars.default_avatar_style, "");
    assert_eq!(config.ui.scrollbar_policy, "");
}

#[test]
fn parses_notifications_section() {
    let config = parse_config_text(
        r#"
notifications:
  enabled: false
  attention_on_incoming: true
  message_content_policy: unencrypted_only
"#,
    );

    assert_eq!(config.notifications.enabled, Some(false));
    assert_eq!(config.notifications.attention_on_incoming, Some(true));
    assert_eq!(config.notifications.message_content_policy, "unencrypted_only");
}

#[test]
fn parses_network_section() {
    let config = parse_config_text(
        r#"
network:
  presence:
    status_policy: offline
  tls:
    enable_certificate_validation: false
  mrs_enabled: false
  mrs_server_name: example.org
  http3_enabled: true
"#,
    );

    assert_eq!(config.network.presence_status_policy, "offline");
    assert_eq!(config.network.tls_enable_certificate_validation, Some(false));
    assert_eq!(config.network.mrs_enabled, Some(false));
    assert_eq!(config.network.mrs_server_name, "example.org");
    assert_eq!(config.network.http3_enabled, Some(true));
}

#[test]
fn parses_integrations_section() {
    let config = parse_config_text(
        r#"
integrations:
  system_tray:
    enabled: true
    autostart: false
  dbus:
    access: read_only
  browser:
    command: firefox %s
"#,
    );

    assert_eq!(config.integrations.system_tray_enabled, Some(true));
    assert_eq!(config.integrations.system_tray_autostart, Some(false));
    assert_eq!(config.integrations.dbus_api_access, "read_only");
    assert_eq!(config.integrations.browser_command, "firefox %s");
}

#[test]
fn encodes_generic_config_values() {
    let yaml = encode_config_yaml(&SettingsConfigSnapshot {
        ui: SettingsConfigUiSection {
            has_scale_factor: false,
            scale_factor: 0.0,
            theme_slug: "komai-dark".to_owned(),
            has_font_size_pt: true,
            font_size_pt: 14.0,
            font_family: "Iosevka".to_owned(),
            font_emoji_family: "Noto Color Emoji".to_owned(),
            has_motion_animations_enabled: true,
            motion_animations_enabled: true,
            input_mode: "desktop".to_owned(),
            has_input_touch_swipe_gestures_enabled: true,
            input_touch_swipe_gestures_enabled: true,
            has_layout_content_max_width_px: true,
            layout_content_max_width_px: 1024,
            has_layout_compact_mode: true,
            layout_compact_mode: false,
            has_avatars_circular: true,
            avatars_circular: true,
            scrollbar_policy: "when_needed".to_owned(),
            default_avatar_style: "boring_avatars_bauhaus".to_owned(),
        },
        timeline: SettingsConfigTimelineSection {
            hidden_events: SettingsConfigTimelineHiddenEventsSection {
                has_global: true,
                global: vec!["m.reaction".to_owned()],
                by_room: vec![SettingsStringListMapEntry {
                    key: "!room:example.org".to_owned(),
                    values: vec!["m.call.candidates".to_owned()],
                }],
            },
        },
        secrets: SettingsConfigSecretsSection {
            provider: "file".to_owned(),
        },
        notifications: SettingsConfigNotificationsSection {
            has_enabled: true,
            enabled: false,
            has_attention_on_incoming: true,
            attention_on_incoming: true,
            message_content_policy: "unencrypted_only".to_owned(),
        },
        network: SettingsConfigNetworkSection {
            presence_status_policy: "offline".to_owned(),
            has_tls_enable_certificate_validation: true,
            tls_enable_certificate_validation: false,
            has_mrs_enabled: true,
            mrs_enabled: false,
            mrs_server_name: "example.org".to_owned(),
            has_http3_enabled: true,
            http3_enabled: true,
        },
        integrations: SettingsConfigIntegrationsSection {
            has_system_tray_enabled: true,
            system_tray_enabled: true,
            has_system_tray_autostart: true,
            system_tray_autostart: false,
            dbus_api_access: "read_only".to_owned(),
            browser_command: "firefox %s".to_owned(),
        },
        values: vec![
            SettingsConfigValue {
                key: "meta.ignored".to_owned(),
                kind: SettingsConfigValueKind::String,
                bool_value: false,
                int_value: 0,
                double_value: 0.0,
                string_value: "x".to_owned(),
                string_list_value: vec![],
                string_list_map_value: vec![],
            },
        ],
    });

    let root: serde_yaml_ng::Value = serde_yaml_ng::from_str(&yaml).expect("valid yaml");
    assert!(matches!(
        yaml::value_at_path(&root, &["meta", "settings_schema_version"]),
        Some(serde_yaml_ng::Value::Number(number)) if number.as_i64() == Some(1)
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "theme", "slug"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "komai-dark"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "hidden_events", "global"]),
        Some(serde_yaml_ng::Value::Sequence(_))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "hidden_events", "by_room"]),
        Some(serde_yaml_ng::Value::Mapping(_))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "input", "mode"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "desktop"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "font", "family"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "Iosevka"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "font", "emoji_family"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "Noto Color Emoji"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "font", "size_pt"]),
        Some(serde_yaml_ng::Value::Number(number)) if number.as_f64() == Some(14.0)
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "motion", "enable_animations"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "input", "touch", "swipe_gestures", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "layout", "content", "max_width_px"]),
        Some(serde_yaml_ng::Value::Number(number)) if number.as_i64() == Some(1024)
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "layout", "compact_mode"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "avatars", "circular"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "avatars", "default_avatar_style"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "boring_avatars_bauhaus"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "scrollbar_policy"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "when_needed"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["secrets", "provider"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "file"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["notifications", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["notifications", "attention_on_incoming"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["notifications", "message_content_policy"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "unencrypted_only"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "presence", "status_policy"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "offline"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "tls", "enable_certificate_validation"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "mrs_enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "mrs_server_name"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "example.org"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "http3_enabled"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["integrations", "system_tray", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["integrations", "system_tray", "autostart"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["integrations", "dbus", "access"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "read_only"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["integrations", "browser", "command"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "firefox %s"
    ));
}

#[test]
fn loaded_snapshot_keeps_typed_overview_fields() {
    let loaded = load_config_snapshot(
        r#"
meta:
  settings_schema_version: 0
ui:
  scale:
    factor: 1.5
  theme:
    slug: komai-dark
secrets:
  provider: file
"#,
    );

    assert_eq!(loaded.config.ui.scale.factor, Some(1.5));
    assert_eq!(loaded.config.ui.theme.slug, "komai-dark");
    assert_eq!(loaded.config.ui.input.mode, "");
    assert_eq!(loaded.config.ui.motion.animations_enabled, None);
    assert_eq!(loaded.config.secrets.provider, "file");
    assert!(loaded.should_write_back);
}

#[test]
fn loaded_snapshot_keeps_future_version_untouched() {
    let loaded = load_config_snapshot(
        r#"
meta:
  settings_schema_version: 8
ui:
  theme:
    slug: komai-dark
"#,
    );

    assert!(loaded.had_future_version);
    assert!(!loaded.should_write_back);
    assert_eq!(loaded.source_version, 8);
    assert_eq!(loaded.migrated_version, 8);
    assert_eq!(loaded.config.ui.theme.slug, "komai-dark");
}

#[test]
fn loaded_snapshot_clamps_negative_schema_version() {
    let loaded = load_config_snapshot(
        r#"
meta:
  settings_schema_version: -5
ui:
  theme:
    slug: komai-dark
"#,
    );

    assert!(!loaded.had_future_version);
    assert_eq!(loaded.source_version, 0);
    assert_eq!(loaded.migrated_version, 1);
    assert!(loaded.should_write_back);
    assert_eq!(loaded.config.ui.theme.slug, "komai-dark");
}

#[test]
fn loaded_snapshot_normalizes_non_map_root() {
    let loaded = load_config_snapshot("\"not-a-map\"");

    assert_eq!(loaded.source_version, 0);
    assert_eq!(loaded.migrated_version, 1);
    assert!(loaded.should_write_back);
    assert!(loaded.values.is_empty());
    assert_eq!(loaded.config.ui.theme.slug, "");
}
