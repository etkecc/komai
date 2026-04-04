// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    encode_config_yaml, load_config_snapshot, parse_config_text,
    ConfigSecretsProviderToken, ConfigUiDefaultAvatarStyleToken, ConfigUiInputModeToken,
    ConfigUiScrollbarPolicyToken,
    ConfigSidebarsRoomListLastMessagePreviewToken, ConfigSidebarsRoomListSortToken,
    ConfigSidebarsRoomListUnreadDetectionPolicyToken, ConfigTimelineMediaImageDisplayToken,
    ConfigTimelineMessageActionsActivationPolicyToken, ConfigTimelineMessagesPositioningToken,
    ConfigTimelineMessagesSenderUsernameToken, ConfigTimelineMessagesStyleToken,
    ConfigTimelineUserColorCodingPolicyToken,
};
use crate::ffi::{
    SettingsConfigComposerSection, SettingsConfigEncryptionBackupOnlineSection,
    SettingsConfigEncryptionBackupSection, SettingsConfigEncryptionKeySharingSection,
    SettingsConfigEncryptionSection, SettingsConfigDesktopAttentionAppBadgeSection,
    SettingsConfigDesktopAttentionSection, SettingsConfigDesktopAttentionWindowTitleSection,
    SettingsConfigDesktopNotificationsSection, SettingsConfigDesktopSection,
    SettingsConfigDesktopSystemTraySection, SettingsConfigDesktopWindowFocusBlurSection,
    SettingsConfigIntegrationsSection, SettingsConfigNetworkSection,
    SettingsConfigSecretsSection, SettingsConfigSidebarsCommunitiesSection,
    SettingsConfigSidebarsRoomListSection, SettingsConfigSidebarsSection,
    SettingsConfigSnapshot, SettingsConfigTimelineFormattedSection,
    SettingsConfigTimelineHiddenEventsSection, SettingsConfigTimelineMaintenanceSection,
    SettingsConfigTimelineMediaSection, SettingsConfigTimelineMessageActionsSection,
    SettingsConfigTimelineMessagesSection, SettingsConfigTimelineReadReceiptsSection,
    SettingsConfigTimelineSection, SettingsConfigTimelineTypingSection, SettingsConfigUiSection,
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
    assert_eq!(config.ui.input.mode, ConfigUiInputModeToken::Text);
    assert_eq!(config.secrets.provider, ConfigSecretsProviderToken::File);
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

    assert_eq!(config.ui.input.mode, ConfigUiInputModeToken::Touch);
    assert_eq!(config.timeline.hidden_events.global, Some(Vec::new()));
    assert_eq!(
        config.timeline.hidden_events.by_room.get("!room:example.org"),
        Some(&vec!["m.call.candidates".to_owned(), "m.reaction".to_owned()])
    );
}

#[test]
fn parses_timeline_section() {
    let config = parse_config_text(
        r#"
timeline:
  messages:
    style: plain
    positioning: all_right
    layout:
      small_avatars: true
      show_own_avatar: false
    sender_username: always
    emoji_only_enlarge: false
    hover_highlight: true
    actions:
      activation_policy: on_message_hover
      pinned_reactions: "👍,👀"
  user_color_coding_policy: me_vs_others
  formatted:
    code_syntax_highlighting: false
  typing:
    show:
      enabled: false
  read_receipts:
    enabled: false
  media:
    effects:
      enabled: false
    animate_on_hover: true
    image_display: never
    open_images_external: true
    open_videos_external: true
    autoplay_gif_videos: false
    open_audio_external: true
    default_audio_playback_speed: 2.5
"#,
    );

    assert_eq!(config.timeline.messages.style, ConfigTimelineMessagesStyleToken::Plain);
    assert_eq!(
        config.timeline.messages.positioning,
        ConfigTimelineMessagesPositioningToken::AllRight
    );
    assert_eq!(
        config.timeline.user_color_coding_policy,
        ConfigTimelineUserColorCodingPolicyToken::MeVsOthers
    );
    assert_eq!(config.timeline.messages.layout.small_avatars, Some(true));
    assert_eq!(config.timeline.messages.layout.show_own_avatar, Some(false));
    assert_eq!(
        config.timeline.messages.sender_username,
        ConfigTimelineMessagesSenderUsernameToken::Always
    );
    assert_eq!(config.timeline.messages.emoji_only_enlarge, Some(false));
    assert_eq!(config.timeline.messages.hover_highlight, Some(true));
    assert_eq!(
        config.timeline.message_actions.activation_policy,
        ConfigTimelineMessageActionsActivationPolicyToken::OnHover
    );
    assert_eq!(config.timeline.message_actions.pinned_reactions, "👍,👀");
    assert_eq!(
        config.timeline.formatted.code_syntax_highlighting,
        Some(false)
    );
    assert_eq!(config.timeline.typing.show_enabled, Some(false));
    assert_eq!(config.timeline.read_receipts.enabled, Some(false));
    assert_eq!(config.timeline.media.effects_enabled, Some(false));
    assert_eq!(config.timeline.media.animate_on_hover, Some(true));
    assert_eq!(config.timeline.media.image_display, ConfigTimelineMediaImageDisplayToken::Never);
    assert_eq!(config.timeline.media.open_images_external, Some(true));
    assert_eq!(config.timeline.media.open_videos_external, Some(true));
    assert_eq!(config.timeline.media.autoplay_gif_videos, Some(false));
    assert_eq!(config.timeline.media.open_audio_external, Some(true));
    assert_eq!(config.timeline.media.default_audio_playback_speed, Some(2.5));
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
    assert_eq!(
        config.ui.avatars.default_avatar_style,
        ConfigUiDefaultAvatarStyleToken::BoringAvatarsBauhaus
    );
    assert_eq!(
        config.ui.scrollbar_policy,
        ConfigUiScrollbarPolicyToken::WhenNeeded
    );
}

#[test]
fn parses_desktop_section() {
    let config = parse_config_text(
        r#"
desktop:
  notifications:
    enabled: false
    attention_on_incoming: true
    message_content_policy: unencrypted_only
  attention:
    window_title:
      enabled: false
    app_badge:
      enabled: true
  system_tray:
    enabled: true
    autostart: false
  window_focus_blur:
    enabled: true
    delay_seconds: 3
"#,
    );

    assert_eq!(config.desktop.notifications.enabled, Some(false));
    assert_eq!(config.desktop.notifications.attention_on_incoming, Some(true));
    assert_eq!(
        config.desktop.notifications.message_content_policy,
        "unencrypted_only".into()
    );
    assert_eq!(config.desktop.attention.window_title.enabled, Some(false));
    assert_eq!(config.desktop.attention.app_badge.enabled, Some(true));
    assert_eq!(config.desktop.system_tray.enabled, Some(true));
    assert_eq!(config.desktop.system_tray.autostart, Some(false));
    assert_eq!(config.desktop.window_focus_blur.enabled, Some(true));
    assert_eq!(config.desktop.window_focus_blur.delay_seconds, Some(3));
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
  mrs:
    enabled: false
    server_name: example.org
  http3:
    enabled: true
"#,
    );

    assert_eq!(config.network.presence_status_policy, "offline".into());
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
  dbus:
    access: read_only
  browser:
    command: firefox %s
"#,
    );

    assert_eq!(config.integrations.dbus_api_access, "read_only".into());
    assert_eq!(config.integrations.browser_command, "firefox %s");
}

#[test]
fn parses_sidebars_section() {
    let config = parse_config_text(
        r#"
sidebars:
  room_list:
    show_last_message_timestamp: false
    last_message_preview: never
    show_community_notification_counts: true
    sort: alphabetical
    unread_detection_policy: any_event
  communities:
    visible: false
    filters:
      favourites: false
      people: true
      bots: false
      groups: true
      server_notices: false
      low_priority: true
"#,
    );

    assert_eq!(config.sidebars.room_list.show_last_message_time, Some(false));
    assert_eq!(
        config.sidebars.room_list.last_message_preview,
        ConfigSidebarsRoomListLastMessagePreviewToken::Never
    );
    assert_eq!(config.sidebars.room_list.show_community_counts, Some(true));
    assert_eq!(
        config.sidebars.room_list.sort,
        ConfigSidebarsRoomListSortToken::Alphabetical
    );
    assert_eq!(
        config.sidebars.room_list.unread_detection_policy,
        ConfigSidebarsRoomListUnreadDetectionPolicyToken::AnyEvent
    );
    assert_eq!(config.sidebars.communities.visible, Some(false));
    assert_eq!(config.sidebars.communities.filter_favourites, Some(false));
    assert_eq!(config.sidebars.communities.filter_people, Some(true));
    assert_eq!(config.sidebars.communities.filter_bots, Some(false));
    assert_eq!(config.sidebars.communities.filter_groups, Some(true));
    assert_eq!(config.sidebars.communities.filter_server_notices, Some(false));
    assert_eq!(config.sidebars.communities.filter_low_priority, Some(true));
}

#[test]
fn parses_composer_section() {
    let config = parse_config_text(
        r#"
composer:
  input:
    markdown_to_html:
      enabled: false
    send_key: ctrl_enter
    auto_replace_emoji: never
    emoji:
      preferred_gender: woman
      preferred_skin_tone: medium_dark
    inline_emoji_picker:
      enabled: false
    inline_room_picker:
      enabled: true
    inline_user_picker:
      enabled: false
  typing:
    send:
      enabled: false
  extras:
    stickers:
      enabled: true
"#,
    );

    assert_eq!(config.composer.input_markdown_to_html_enabled, Some(false));
    assert_eq!(config.composer.input_send_key, "ctrl_enter".into());
    assert_eq!(config.composer.input_auto_replace_emoji, "never".into());
    assert_eq!(config.composer.input_emoji_preferred_gender, "woman".into());
    assert_eq!(config.composer.input_emoji_preferred_skin_tone, "medium_dark".into());
    assert_eq!(config.composer.input_inline_emoji_picker_enabled, Some(false));
    assert_eq!(config.composer.input_inline_room_picker_enabled, Some(true));
    assert_eq!(config.composer.input_inline_user_picker_enabled, Some(false));
    assert_eq!(config.composer.typing_send_enabled, Some(false));
    assert_eq!(config.composer.extras_stickers_enabled, Some(true));
}

#[test]
fn parses_encryption_section() {
    let config = parse_config_text(
        r#"
encryption:
  key_sharing:
    only_verified_users: true
    share_with_trusted: true
  backup:
    online:
      enabled: false
"#,
    );

    assert_eq!(config.encryption.key_sharing.only_verified_users, Some(true));
    assert_eq!(config.encryption.key_sharing.share_with_trusted, Some(true));
    assert_eq!(config.encryption.backup.online.enabled, Some(false));
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
        sidebars: SettingsConfigSidebarsSection {
            room_list: SettingsConfigSidebarsRoomListSection {
                has_show_last_message_time: true,
                show_last_message_time: false,
                last_message_preview: "never".to_owned(),
                has_show_community_counts: true,
                show_community_counts: true,
                sort: "alphabetical".to_owned(),
                unread_detection_policy: "any_event".to_owned(),
            },
            communities: SettingsConfigSidebarsCommunitiesSection {
                has_visible: true,
                visible: false,
                has_filter_favourites: true,
                filter_favourites: false,
                has_filter_people: true,
                filter_people: true,
                has_filter_bots: true,
                filter_bots: false,
                has_filter_groups: true,
                filter_groups: true,
                has_filter_server_notices: true,
                filter_server_notices: false,
                has_filter_low_priority: true,
                filter_low_priority: true,
            },
        },
        timeline: SettingsConfigTimelineSection {
            messages: SettingsConfigTimelineMessagesSection {
                style: "plain".to_owned(),
                positioning: "all_right".to_owned(),
                user_color_coding_policy: "me_vs_others".to_owned(),
                has_layout_small_avatars: true,
                layout_small_avatars: true,
                has_layout_show_own_avatar: true,
                layout_show_own_avatar: false,
                sender_username: "always".to_owned(),
                has_emoji_only_enlarge: true,
                emoji_only_enlarge: false,
                has_hover_highlight: true,
                hover_highlight: true,
            },
            formatted: SettingsConfigTimelineFormattedSection {
                has_code_syntax_highlighting: true,
                code_syntax_highlighting: false,
            },
            typing: SettingsConfigTimelineTypingSection {
                has_show_enabled: true,
                show_enabled: false,
            },
            read_receipts: SettingsConfigTimelineReadReceiptsSection {
                has_enabled: true,
                enabled: false,
            },
            message_actions: SettingsConfigTimelineMessageActionsSection {
                activation_policy: "on_message_hover".to_owned(),
                pinned_reactions: "👍,👀".to_owned(),
            },
            media: SettingsConfigTimelineMediaSection {
                has_effects_enabled: true,
                effects_enabled: false,
                has_animate_on_hover: true,
                animate_on_hover: true,
                image_display: "never".to_owned(),
                has_open_images_external: true,
                open_images_external: true,
                has_open_videos_external: true,
                open_videos_external: true,
                has_autoplay_gif_videos: true,
                autoplay_gif_videos: false,
                has_open_audio_external: true,
                open_audio_external: true,
                has_default_audio_playback_speed: true,
                default_audio_playback_speed: 2.5,
            },
            hidden_events: SettingsConfigTimelineHiddenEventsSection {
                has_global: true,
                global: vec!["m.reaction".to_owned()],
                by_room: vec![SettingsStringListMapEntry {
                    key: "!room:example.org".to_owned(),
                    values: vec!["m.call.candidates".to_owned()],
                }],
            },
            maintenance: SettingsConfigTimelineMaintenanceSection {
                has_expire_events: true,
                expire_events: false,
            },
        },
        secrets: SettingsConfigSecretsSection {
            provider: "file".to_owned(),
        },
        desktop: SettingsConfigDesktopSection {
            notifications: SettingsConfigDesktopNotificationsSection {
                has_enabled: true,
                enabled: false,
                has_attention_on_incoming: true,
                attention_on_incoming: true,
                message_content_policy: "unencrypted_only".to_owned(),
            },
            attention: SettingsConfigDesktopAttentionSection {
                window_title: SettingsConfigDesktopAttentionWindowTitleSection {
                    has_enabled: true,
                    enabled: false,
                },
                app_badge: SettingsConfigDesktopAttentionAppBadgeSection {
                    has_enabled: true,
                    enabled: true,
                },
            },
            system_tray: SettingsConfigDesktopSystemTraySection {
                has_enabled: true,
                enabled: true,
                has_autostart: true,
                autostart: false,
            },
            window_focus_blur: SettingsConfigDesktopWindowFocusBlurSection {
                has_enabled: true,
                enabled: false,
                has_delay_seconds: true,
                delay_seconds: 3,
            },
        },
        encryption: SettingsConfigEncryptionSection {
            key_sharing: SettingsConfigEncryptionKeySharingSection {
                has_only_verified_users: true,
                only_verified_users: true,
                has_share_with_trusted: true,
                share_with_trusted: true,
            },
            backup: SettingsConfigEncryptionBackupSection {
                online: SettingsConfigEncryptionBackupOnlineSection {
                    has_enabled: true,
                    enabled: false,
                },
            },
        },
        calls: crate::ffi::SettingsConfigCallsSection {
            legacy: crate::ffi::SettingsConfigCallsLegacySection {
                has_enabled: true,
                enabled: true,
            },
            relay: crate::ffi::SettingsConfigCallsRelaySection {
                has_use_fallback_server: true,
                use_fallback_server: false,
            },
            devices: crate::ffi::SettingsConfigCallsDevicesSection {
                microphone: "default".to_owned(),
                camera: "default".to_owned(),
                camera_resolution: "hd".to_owned(),
                camera_frame_rate: "30".to_owned(),
            },
            audio: crate::ffi::SettingsConfigCallsAudioSection {
                ringtone: "default".to_owned(),
            },
            screenshare: crate::ffi::SettingsConfigCallsScreenshareSection {
                has_frame_rate: true,
                frame_rate: 15,
                has_picture_in_picture: true,
                picture_in_picture: false,
                has_include_remote_video: true,
                include_remote_video: true,
                has_show_cursor: true,
                show_cursor: true,
            },
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
            dbus_api_access: "read_only".to_owned(),
            browser_command: "firefox %s".to_owned(),
        },
        composer: SettingsConfigComposerSection {
            has_input_markdown_to_html_enabled: true,
            input_markdown_to_html_enabled: false,
            input_send_key: "ctrl_enter".to_owned(),
            input_auto_replace_emoji: "never".to_owned(),
            input_emoji_preferred_gender: "woman".to_owned(),
            input_emoji_preferred_skin_tone: "medium_dark".to_owned(),
            has_input_inline_emoji_picker_enabled: true,
            input_inline_emoji_picker_enabled: false,
            has_input_inline_room_picker_enabled: true,
            input_inline_room_picker_enabled: true,
            has_input_inline_user_picker_enabled: true,
            input_inline_user_picker_enabled: false,
            has_typing_send_enabled: true,
            typing_send_enabled: false,
            has_extras_stickers_enabled: true,
            extras_stickers_enabled: true,
        },
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
        yaml::value_at_path(&root, &["timeline", "messages", "style"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "plain"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "positioning"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "all_right"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "user_color_coding_policy"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "me_vs_others"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "layout", "small_avatars"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "layout", "show_own_avatar"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "sender_username"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "always"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "emoji_only_enlarge"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "hover_highlight"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(
            &root,
            &["timeline", "messages", "actions", "activation_policy"]
        ),
        Some(serde_yaml_ng::Value::String(value)) if value == "on_message_hover"
    ));
    assert!(matches!(
        yaml::value_at_path(
            &root,
            &["timeline", "messages", "actions", "pinned_reactions"]
        ),
        Some(serde_yaml_ng::Value::String(value)) if value == "👍,👀"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "formatted", "code_syntax_highlighting"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "typing", "show", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "read_receipts", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "media", "effects", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "media", "animate_on_hover"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "media", "image_display"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "never"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "media", "open_images_external"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "media", "open_videos_external"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "media", "autoplay_gif_videos"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "media", "open_audio_external"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(
            &root,
            &["timeline", "media", "default_audio_playback_speed"]
        ),
        Some(serde_yaml_ng::Value::Number(number)) if number.as_f64() == Some(2.5)
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
        yaml::value_at_path(&root, &["sidebars", "room_list", "show_last_message_timestamp"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["sidebars", "room_list", "last_message_preview"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "never"
    ));
    assert!(matches!(
        yaml::value_at_path(
            &root,
            &["sidebars", "room_list", "show_community_notification_counts"]
        ),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["sidebars", "room_list", "sort"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "alphabetical"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["sidebars", "room_list", "unread_detection_policy"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "any_event"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["sidebars", "communities", "visible"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["sidebars", "communities", "filters", "favourites"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["sidebars", "communities", "filters", "people"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["sidebars", "communities", "filters", "bots"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["sidebars", "communities", "filters", "groups"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(
            &root,
            &["sidebars", "communities", "filters", "server_notices"]
        ),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["sidebars", "communities", "filters", "low_priority"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["secrets", "provider"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "file"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["encryption", "key_sharing", "only_verified_users"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["encryption", "key_sharing", "share_with_trusted"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["encryption", "backup", "online", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "maintenance", "expire_events"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["desktop", "notifications", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["desktop", "notifications", "attention_on_incoming"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["desktop", "notifications", "message_content_policy"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "unencrypted_only"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["desktop", "attention", "window_title", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["desktop", "attention", "app_badge", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["desktop", "system_tray", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["desktop", "system_tray", "autostart"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["desktop", "window_focus_blur", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["desktop", "window_focus_blur", "delay_seconds"]),
        Some(serde_yaml_ng::Value::Number(number)) if number.as_i64() == Some(3)
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
        yaml::value_at_path(&root, &["network", "mrs", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "mrs", "server_name"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "example.org"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "http3", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["integrations", "dbus", "access"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "read_only"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["integrations", "browser", "command"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "firefox %s"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "input", "markdown_to_html", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "input", "send_key"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "ctrl_enter"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "input", "auto_replace_emoji"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "never"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "input", "emoji", "preferred_gender"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "woman"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "input", "emoji", "preferred_skin_tone"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "medium_dark"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "input", "inline_emoji_picker", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "input", "inline_room_picker", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "input", "inline_user_picker", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "typing", "send", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "extras", "stickers", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(true))
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
    assert_eq!(loaded.config.ui.input.mode, ConfigUiInputModeToken::Text);
    assert_eq!(loaded.config.ui.motion.animations_enabled, None);
    assert_eq!(loaded.config.secrets.provider, ConfigSecretsProviderToken::File);
    assert!(loaded.should_write_back);
}

#[test]
fn invalid_tokens_normalize_to_known_defaults() {
    let config = parse_config_text(
        r#"
ui:
  input:
    mode: nonsense
  avatars:
    default_avatar_style: mystery
  scrollbar_policy: odd
network:
  presence:
    status_policy: ????
sidebars:
  room_list:
    unread_detection_policy: impossible
integrations:
  dbus:
    access: unexpected
composer:
  input:
    send_key: weird
    auto_replace_emoji: nonsense
    emoji:
      preferred_gender: mystery
      preferred_skin_tone: mystery
"#,
    );

    assert_eq!(config.ui.input.mode.to_storage_string(), "text");
    assert_eq!(
        config.ui.avatars.default_avatar_style.to_storage_string(),
        "boring_avatars_bauhaus"
    );
    assert_eq!(config.ui.scrollbar_policy.to_storage_string(), "when_needed");
    assert_eq!(
        config.network.presence_status_policy.to_storage_string(),
        "automatic_presence"
    );
    assert_eq!(config.integrations.dbus_api_access.to_storage_string(), "none");
    assert_eq!(
        config.sidebars.room_list.unread_detection_policy.to_storage_string(),
        "any_event"
    );
    assert_eq!(config.composer.input_send_key.to_storage_string(), "enter");
    assert_eq!(
        config.composer.input_auto_replace_emoji.to_storage_string(),
        "always"
    );
    assert_eq!(
        config.composer.input_emoji_preferred_gender.to_storage_string(),
        "no_preference"
    );
    assert_eq!(
        config.composer.input_emoji_preferred_skin_tone.to_storage_string(),
        "no_preference"
    );
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
    assert_eq!(loaded.config.ui.theme.slug, "");
}
