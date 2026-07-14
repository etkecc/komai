// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use super::{
    encode_config_yaml, load_config_snapshot, parse_config_text,
    ConfigSecretsProviderToken, ConfigUiDefaultAvatarStyleToken, ConfigUiLayoutDensityToken,
    ConfigUiScrollbarPolicyToken, ConfigUiThemeModeToken,
    ConfigNavigationRoomListLastMessagePreviewToken, ConfigNavigationRoomListSortToken,
    ConfigNavigationRoomListOpeningPolicyToken,
    ConfigTimelineMediaImageDisplayToken,
    ConfigTimelineMessageActionsActivationPolicyToken,
    ConfigTimelineMessagesLayoutAvatarSizeToken, ConfigTimelineMessagesPositioningToken,
    ConfigTimelineMessagesSenderUsernameToken, ConfigTimelineMessagesStyleToken,
    ConfigTimelineUserColorCodingPolicyToken,
};
use crate::ffi::{
    SettingsConfigComposerSection, SettingsConfigDesktopAttentionAppBadgeSection,
    SettingsConfigDesktopAttentionSection, SettingsConfigDesktopAttentionWindowTitleSection,
    SettingsConfigDesktopNotificationsSection, SettingsConfigDesktopSection,
    SettingsConfigDesktopSystemTraySection, SettingsConfigDesktopWindowFocusBlurSection,
    SettingsConfigIntegrationsSection, SettingsConfigNetworkEncryptionSection,
    SettingsConfigNetworkSection,
    SettingsConfigSecretsSection, SettingsConfigNavigationCommunitiesSection,
    SettingsConfigNavigationRoomListSection, SettingsConfigNavigationSection, SettingsConfigNavigationTabsSection,
    SettingsConfigSnapshot, SettingsConfigTimelineDateDividersSection,
    SettingsConfigTimelineFormattedSection,
    SettingsConfigTimelineHiddenEventsSection,
    SettingsConfigTimelineMediaSection, SettingsConfigTimelineMessageActionsSection,
    SettingsConfigTimelineMessagesSection, SettingsConfigTimelineReadReceiptsSection,
    SettingsConfigTimelineRoomHeaderSection,
    SettingsConfigTimelineSection, SettingsConfigTimelineThreadsSection,
    SettingsConfigTimelineTypingSection, SettingsConfigTranscriptionByRoomEntry,
    SettingsConfigUiSection,
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
    slug: dark-komai
secrets:
  provider: file
"#,
    );

    assert_eq!(config.ui.theme.slug, "dark-komai");
    assert_eq!(config.secrets.provider, ConfigSecretsProviderToken::File);
}

#[test]
fn parses_optional_theme_mode() {
    let config = parse_config_text(
        r#"
ui:
  theme:
    slug: dark-nord
    mode: dark
"#,
    );
    assert_eq!(config.ui.theme.mode, Some(ConfigUiThemeModeToken::Dark));

    // Absent is None, not a default. Load-bearing: the C++ migration reads None
    // as "profile predates the mode key" and derives the mode from the slug.
    let config = parse_config_text(
        r#"
ui:
  theme:
    slug: light-komai
"#,
    );
    assert_eq!(config.ui.theme.mode, None);
}

#[test]
fn parses_hidden_events() {
    let config = parse_config_text(
        r#"
timeline:
  hidden_events:
    global: []
    by_room:
      "!room:example.org":
        - m.call.candidates
        - m.reaction
"#,
    );

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
    layout:
      positioning: all_right
      avatar_size: small
      show_own_avatar: false
      max_width_percent: 70
    sender_username: always
    emoji_only_enlarge: false
    hover_highlight: true
    drag_select: true
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
    global: false
    by_room:
      "!rr1:server.tld": true
      "!rr2:server.tld": false
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
  date_dividers:
    enabled: false
"#,
    );

    assert_eq!(config.timeline.messages.style, ConfigTimelineMessagesStyleToken::Plain);
    assert_eq!(
        config.timeline.messages.layout.positioning,
        ConfigTimelineMessagesPositioningToken::AllRight
    );
    assert_eq!(
        config.timeline.user_color_coding_policy,
        ConfigTimelineUserColorCodingPolicyToken::MeVsOthers
    );
    assert_eq!(config.timeline.messages.layout.avatar_size, ConfigTimelineMessagesLayoutAvatarSizeToken::Small);
    assert_eq!(config.timeline.messages.layout.show_own_avatar, Some(false));
    assert_eq!(config.timeline.messages.layout.max_width_percent, Some(70));
    assert_eq!(
        config.timeline.messages.sender_username,
        ConfigTimelineMessagesSenderUsernameToken::Always
    );
    assert_eq!(config.timeline.messages.emoji_only_enlarge, Some(false));
    assert_eq!(config.timeline.messages.hover_highlight, Some(true));
    assert_eq!(config.timeline.messages.drag_select, Some(true));
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
    assert_eq!(config.timeline.read_receipts.global, Some(false));
    assert_eq!(
        config.timeline.read_receipts.by_room.get("!rr1:server.tld"),
        Some(&true)
    );
    assert_eq!(
        config.timeline.read_receipts.by_room.get("!rr2:server.tld"),
        Some(&false)
    );
    assert_eq!(config.timeline.media.effects_enabled, Some(false));
    assert_eq!(config.timeline.media.animate_on_hover, Some(true));
    assert_eq!(config.timeline.media.image_display, ConfigTimelineMediaImageDisplayToken::Never);
    assert_eq!(config.timeline.media.open_images_external, Some(true));
    assert_eq!(config.timeline.media.open_videos_external, Some(true));
    assert_eq!(config.timeline.media.autoplay_gif_videos, Some(false));
    assert_eq!(config.timeline.media.open_audio_external, Some(true));
    assert_eq!(config.timeline.media.default_audio_playback_speed, Some(2.5));
    assert_eq!(config.timeline.date_dividers.enabled, Some(false));
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
  layout:
    density: compact
  avatars:
    circular: true
"#,
    );

    assert_eq!(config.ui.font.family, "Iosevka");
    assert_eq!(config.ui.font.emoji_family, "Noto Color Emoji");
    assert_eq!(config.ui.font.size_pt, Some(14.0));
    assert_eq!(config.ui.motion.animations_enabled, Some(true));
    assert_eq!(config.ui.layout.density, ConfigUiLayoutDensityToken::Compact);
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
fn parses_navigation_section() {
    let config = parse_config_text(
        r#"
navigation:
  room_list:
    show_last_message_timestamp: false
    last_message_preview: never
    show_unread_indicators: true
    sort: alphabetical
    opening_policy: reuse_active_tab
  communities:
    show_unread_indicators: true
    filters:
      favourites: false
      people: true
      bots: false
      groups: true
      server_notices: false
      low_priority: true
"#,
    );

    assert_eq!(config.navigation.room_list.show_last_message_time, Some(false));
    assert_eq!(
        config.navigation.room_list.last_message_preview,
        ConfigNavigationRoomListLastMessagePreviewToken::Never
    );
    assert_eq!(config.navigation.room_list.show_unread_indicators, Some(true));
    assert_eq!(
        config.navigation.room_list.sort,
        ConfigNavigationRoomListSortToken::Alphabetical
    );
    assert_eq!(
        config.navigation.room_list.opening_policy,
        ConfigNavigationRoomListOpeningPolicyToken::ReuseActiveTab
    );
    assert_eq!(config.navigation.communities.show_unread_indicators, Some(true));
    assert_eq!(config.navigation.communities.filter_favourites, Some(false));
    assert_eq!(config.navigation.communities.filter_people, Some(true));
    assert_eq!(config.navigation.communities.filter_bots, Some(false));
    assert_eq!(config.navigation.communities.filter_groups, Some(true));
    assert_eq!(config.navigation.communities.filter_server_notices, Some(false));
    assert_eq!(config.navigation.communities.filter_low_priority, Some(true));
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
    transcription:
      enabled: false
  attachments:
    strip_image_metadata: false
  typing:
    send:
      global: false
      by_room:
        "!room1:server.tld": true
        "!room2:server.tld": false
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
    assert_eq!(config.composer.input_transcription_enabled, Some(false));
    assert_eq!(config.composer.attachments_strip_image_metadata, Some(false));
    assert_eq!(config.composer.typing_send.global, Some(false));
    assert_eq!(
        config.composer.typing_send.by_room.get("!room1:server.tld"),
        Some(&true)
    );
    assert_eq!(
        config.composer.typing_send.by_room.get("!room2:server.tld"),
        Some(&false)
    );
}

#[test]
fn parses_composer_typing_send_legacy_enabled_path_for_v1_compat() {
    // v1 stored the global at `composer.typing.send.enabled`. v2 stores it
    // at `composer.typing.send.global` (sibling of the new `by_room` map).
    // The parser falls back to the legacy path so v1 configs round-trip
    // their value through the v1→v2 migration without loss.
    let config = parse_config_text(
        r#"
composer:
  typing:
    send:
      enabled: false
"#,
    );

    assert_eq!(config.composer.typing_send.global, Some(false));
    assert!(config.composer.typing_send.by_room.is_empty());
}

#[test]
fn parses_composer_typing_send_global_takes_precedence_over_legacy_enabled() {
    // When both forms coexist (mid-migration / hand-edited file), the v2
    // `global` key wins. The legacy `enabled` key gets dropped on the
    // next snapshot write because the encoder only emits `global` and
    // `by_room`.
    let config = parse_config_text(
        r#"
composer:
  typing:
    send:
      enabled: true
      global: false
"#,
    );

    assert_eq!(config.composer.typing_send.global, Some(false));
}

#[test]
fn parses_network_encryption_section() {
    let config = parse_config_text(
        r#"
network:
  encryption:
    only_verified_users: true
    share_with_trusted: true
    key_backup: false
"#,
    );

    assert_eq!(config.network.encryption.only_verified_users, Some(true));
    assert_eq!(config.network.encryption.share_with_trusted, Some(true));
    assert_eq!(config.network.encryption.key_backup, Some(false));
}

#[test]
fn encodes_generic_config_values() {
    let yaml = encode_config_yaml(&SettingsConfigSnapshot {
        ui: SettingsConfigUiSection {
            scale_factor: 0.0,
            theme_slug: "dark-komai".to_owned(),
            theme_mode: String::new(),
            font_size_pt: 14.0,
            font_family: "Iosevka".to_owned(),
            font_emoji_family: "Noto Color Emoji".to_owned(),
            motion_animations_enabled: true,
            layout_density: "spacious".to_owned(),
            avatars_circular: true,
            scrollbar_policy: "when_needed".to_owned(),
            default_avatar_style: "boring_avatars_bauhaus".to_owned(),
            language: String::new(),
        },
        navigation: SettingsConfigNavigationSection {
            room_list: SettingsConfigNavigationRoomListSection {
                show_last_message_time: false,
                last_message_preview: "never".to_owned(),
                show_unread_indicators: true,
                sort: "alphabetical".to_owned(),
                opening_policy: "reuse_active_tab".to_owned(),
            },
            communities: SettingsConfigNavigationCommunitiesSection {
                show_unread_indicators: true,
                filter_favourites: false,
                filter_people: true,
                filter_bots: false,
                filter_groups: true,
                filter_server_notices: false,
                filter_low_priority: true,
            },
            tabs: SettingsConfigNavigationTabsSection {
                auto_hide_with_single_tab: false,
                show_pin_button: "never".to_owned(),
                pinned_tab_label: "avatar_and_label".to_owned(),
                tab_label: "avatar_and_label".to_owned(),
                preferred_width_px: 0,
                minimum_width_px: 0,
                max_recently_closed_timelines: 0,
            },
        },
        timeline: SettingsConfigTimelineSection {
            messages: SettingsConfigTimelineMessagesSection {
                style: "plain".to_owned(),
                layout_positioning: "all_right".to_owned(),
                user_color_coding_policy: "me_vs_others".to_owned(),
                layout_avatar_size: "small".to_owned(),
                layout_show_own_avatar: false,
                layout_max_width_percent: 70,
                layout_adaptive_positioning_breakpoint_px: 2000,
                sender_username: "always".to_owned(),
                emoji_only_enlarge: false,
                hover_highlight: true,
                drag_select: true,
            },
            formatted: SettingsConfigTimelineFormattedSection {
                code_syntax_highlighting: false,
            },
            typing: SettingsConfigTimelineTypingSection {
                show_enabled: false,
            },
            read_receipts: SettingsConfigTimelineReadReceiptsSection {
                global: false,
                by_room: vec![],
            },
            message_actions: SettingsConfigTimelineMessageActionsSection {
                activation_policy: "on_message_hover".to_owned(),
                pinned_reactions: "👍,👀".to_owned(),
            },
            media: SettingsConfigTimelineMediaSection {
                effects_enabled: false,
                animate_on_hover: true,
                image_display: "never".to_owned(),
                open_images_external: true,
                open_videos_external: true,
                autoplay_gif_videos: false,
                open_audio_external: true,
                default_audio_playback_speed: 2.5,
            },
            hidden_events: SettingsConfigTimelineHiddenEventsSection {
                global: vec!["m.reaction".to_owned()],
                by_room: vec![SettingsStringListMapEntry {
                    key: "!room:example.org".to_owned(),
                    values: vec!["m.call.candidates".to_owned()],
                }],
            },
            threads: SettingsConfigTimelineThreadsSection {
                collapse_replies_global: false,
                collapse_replies_by_room: vec![],
            },
            date_dividers: SettingsConfigTimelineDateDividersSection {
                enabled: true,
            },
            room_header: SettingsConfigTimelineRoomHeaderSection {
                button_labels: "adaptive".to_owned(),
            },
        },
        secrets: SettingsConfigSecretsSection {
            provider: "file".to_owned(),
        },
        desktop: SettingsConfigDesktopSection {
            notifications: SettingsConfigDesktopNotificationsSection {
                enabled: false,
                attention_on_incoming: true,
                message_content_policy: "unencrypted_only".to_owned(),
            },
            attention: SettingsConfigDesktopAttentionSection {
                window_title: SettingsConfigDesktopAttentionWindowTitleSection {
                    enabled: false,
                },
                app_badge: SettingsConfigDesktopAttentionAppBadgeSection {
                    enabled: true,
                },
            },
            system_tray: SettingsConfigDesktopSystemTraySection {
                enabled: true,
                autostart: false,
                icon_style: "colorized".to_owned(),
            },
            window_focus_blur: SettingsConfigDesktopWindowFocusBlurSection {
                enabled: false,
                delay_seconds: 3,
            },
        },
        calls: crate::ffi::SettingsConfigCallsSection {
            legacy: crate::ffi::SettingsConfigCallsLegacySection {
                enabled: true,
            },
            element: crate::ffi::SettingsConfigCallsElementSection {
                enabled: true,
            },
            relay: crate::ffi::SettingsConfigCallsRelaySection {
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
                frame_rate: 15,
                picture_in_picture: false,
                include_remote_video: true,
                show_cursor: true,
            },
        },
        network: SettingsConfigNetworkSection {
            encryption: SettingsConfigNetworkEncryptionSection {
                only_verified_users: true,
                share_with_trusted: true,
                key_backup: false,
            },
            presence_status_policy: "offline".to_owned(),
            tls_enable_certificate_validation: false,
            mrs_enabled: false,
            mrs_server_name: "example.org".to_owned(),
            http3_enabled: true,
        },
        integrations: SettingsConfigIntegrationsSection {
            dbus_api_access: "read_only".to_owned(),
            browser_command: "firefox %s".to_owned(),
            transcription_provider: "openai_batch".to_owned(),
            transcription_api_url: "https://api.openai.com/v1".to_owned(),
            transcription_model: "whisper-1".to_owned(),
            transcription_language: "en".to_owned(),
            transcription_prompt: "Komai chat".to_owned(),
            transcription_by_room: vec![],
        },
        composer: SettingsConfigComposerSection {
            input_markdown_to_html_enabled: false,
            input_send_key: "ctrl_enter".to_owned(),
            input_auto_replace_emoji: "never".to_owned(),
            input_emoji_preferred_gender: "woman".to_owned(),
            input_emoji_preferred_skin_tone: "medium_dark".to_owned(),
            input_inline_emoji_picker_enabled: false,
            input_inline_room_picker_enabled: true,
            input_inline_user_picker_enabled: false,
            input_selection_formatting_toolbar_enabled: false,
            input_transcription_enabled: false,
            input_spellcheck_enabled: false,
            input_spellcheck_languages: vec!["bg_BG".to_owned(), "en_US".to_owned()],
            attachments_strip_image_metadata: false,
            typing_send_global: false,
            typing_send_by_room: vec![],
        },
    });

    let root: serde_yaml_ng::Value = serde_yaml_ng::from_str(&yaml).expect("valid yaml");
    assert!(matches!(
        yaml::value_at_path(&root, &["meta", "settings_schema_version"]),
        Some(serde_yaml_ng::Value::Number(number)) if number.as_i64() == Some(3)
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["ui", "theme", "slug"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "dark-komai"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "style"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "plain"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "layout", "positioning"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "all_right"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "user_color_coding_policy"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "me_vs_others"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "layout", "avatar_size"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "small"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "layout", "show_own_avatar"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "messages", "layout", "max_width_percent"]),
        Some(serde_yaml_ng::Value::Number(number)) if number.as_i64() == Some(70)
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
        yaml::value_at_path(&root, &["timeline", "read_receipts", "global"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    // Encoder always emits an empty by_room map alongside the global,
    // mirroring `composer.typing.send.by_room`. The legacy
    // `timeline.read_receipts.enabled` v2 path must not appear in v3 output.
    assert!(matches!(
        yaml::value_at_path(&root, &["timeline", "read_receipts", "by_room"]),
        Some(serde_yaml_ng::Value::Mapping(mapping)) if mapping.is_empty()
    ));
    assert!(yaml::value_at_path(&root, &["timeline", "read_receipts", "enabled"]).is_none());
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
        yaml::value_at_path(&root, &["ui", "layout", "density"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "spacious"
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
        yaml::value_at_path(&root, &["navigation", "room_list", "show_last_message_timestamp"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["navigation", "room_list", "last_message_preview"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "never"
    ));
    assert!(matches!(
        yaml::value_at_path(
            &root,
            &["navigation", "room_list", "show_unread_indicators"]
        ),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["navigation", "room_list", "sort"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "alphabetical"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["navigation", "room_list", "opening_policy"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "reuse_active_tab"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["navigation", "communities", "show_unread_indicators"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["navigation", "communities", "filters", "favourites"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["navigation", "communities", "filters", "people"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["navigation", "communities", "filters", "bots"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["navigation", "communities", "filters", "groups"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(
            &root,
            &["navigation", "communities", "filters", "server_notices"]
        ),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["navigation", "communities", "filters", "low_priority"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["secrets", "provider"]),
        Some(serde_yaml_ng::Value::String(value)) if value == "file"
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "encryption", "only_verified_users"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "encryption", "share_with_trusted"]),
        Some(serde_yaml_ng::Value::Bool(true))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["network", "encryption", "key_backup"]),
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
        yaml::value_at_path(&root, &["composer", "input", "transcription", "enabled"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "typing", "send", "global"]),
        Some(serde_yaml_ng::Value::Bool(false))
    ));
    // The encoder always emits an empty by_room map alongside the global,
    // mirroring `timeline.threads.collapse_replies.by_room`. The legacy
    // `composer.typing.send.enabled` v1 path must not appear in v2 output.
    assert!(matches!(
        yaml::value_at_path(&root, &["composer", "typing", "send", "by_room"]),
        Some(serde_yaml_ng::Value::Mapping(mapping)) if mapping.is_empty()
    ));
    assert!(yaml::value_at_path(&root, &["composer", "typing", "send", "enabled"]).is_none());
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
    slug: dark-komai
secrets:
  provider: file
"#,
    );

    assert_eq!(loaded.config.ui.scale.factor, Some(1.5));
    assert_eq!(loaded.config.ui.theme.slug, "dark-komai");
    assert_eq!(loaded.config.ui.motion.animations_enabled, None);
    assert_eq!(loaded.config.secrets.provider, ConfigSecretsProviderToken::File);
    assert!(loaded.should_write_back);
}

#[test]
fn invalid_tokens_normalize_to_known_defaults() {
    let config = parse_config_text(
        r#"
ui:
  avatars:
    default_avatar_style: mystery
  scrollbar_policy: odd
network:
  presence:
    status_policy: ????
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
    slug: dark-komai
"#,
    );

    assert!(loaded.had_future_version);
    assert!(!loaded.should_write_back);
    assert_eq!(loaded.source_version, 8);
    assert_eq!(loaded.migrated_version, 8);
    assert_eq!(loaded.config.ui.theme.slug, "dark-komai");
}

#[test]
fn loaded_snapshot_clamps_negative_schema_version() {
    let loaded = load_config_snapshot(
        r#"
meta:
  settings_schema_version: -5
ui:
  theme:
    slug: dark-komai
"#,
    );

    assert!(!loaded.had_future_version);
    assert_eq!(loaded.source_version, 0);
    assert_eq!(loaded.migrated_version, 3);
    assert!(loaded.should_write_back);
    assert_eq!(loaded.config.ui.theme.slug, "dark-komai");
}

#[test]
fn loaded_snapshot_normalizes_non_map_root() {
    let loaded = load_config_snapshot("\"not-a-map\"");

    assert_eq!(loaded.source_version, 0);
    assert_eq!(loaded.migrated_version, 3);
    assert!(loaded.should_write_back);
    assert_eq!(loaded.config.ui.theme.slug, "");
}

#[test]
fn migrates_composer_typing_send_v1_to_v2() {
    // A v1 config carries `composer.typing.send.enabled` as a bool leaf.
    // The v2 schema places the global toggle at
    // `composer.typing.send.global` so it can sit alongside a sibling
    // `by_room` map (mirroring `timeline.threads.collapse_replies`).
    // Loading a v1 file must:
    //   1. preserve the existing global value (via the legacy-path fallback)
    //   2. mark the snapshot for write-back so the next save lands at v2
    //   3. stamp the migrated_version at the new current value so the
    //      writer triggers the on-disk schema bump
    //
    // The legacy `enabled` key is dropped on the next save because
    // `encode_config_yaml` only emits `global`+`by_room` — that is asserted
    // by `encodes_full_config_to_yaml` above.
    let loaded = load_config_snapshot(
        r#"
meta:
  settings_schema_version: 1
composer:
  typing:
    send:
      enabled: false
"#,
    );

    assert_eq!(loaded.source_version, 1);
    assert_eq!(loaded.migrated_version, 3);
    assert!(loaded.should_write_back);
    assert_eq!(loaded.config.composer.typing_send.global, Some(false));
    assert!(loaded.config.composer.typing_send.by_room.is_empty());
}

#[test]
fn migrates_timeline_read_receipts_v2_to_v3() {
    // A v2 config carries `timeline.read_receipts.enabled` as a bool leaf.
    // The v3 schema places the global toggle at
    // `timeline.read_receipts.global` so it can sit alongside a sibling
    // `by_room` map (mirroring `composer.typing.send`). Loading a v2 file
    // must preserve the existing global value via the legacy-path
    // fallback and stamp the snapshot at the new current schema version.
    let loaded = load_config_snapshot(
        r#"
meta:
  settings_schema_version: 2
timeline:
  read_receipts:
    enabled: false
"#,
    );

    assert_eq!(loaded.source_version, 2);
    assert_eq!(loaded.migrated_version, 3);
    assert!(loaded.should_write_back);
    assert_eq!(loaded.config.timeline.read_receipts.global, Some(false));
    assert!(loaded.config.timeline.read_receipts.by_room.is_empty());
}

#[test]
fn parses_timeline_read_receipts_legacy_enabled_path_for_v2_compat() {
    let config = parse_config_text(
        r#"
timeline:
  read_receipts:
    enabled: false
"#,
    );

    assert_eq!(config.timeline.read_receipts.global, Some(false));
    assert!(config.timeline.read_receipts.by_room.is_empty());
}

#[test]
fn parses_timeline_read_receipts_global_takes_precedence_over_legacy_enabled() {
    // When both forms coexist (mid-migration / hand-edited file), the v3
    // `global` key wins. The legacy `enabled` key gets dropped on the
    // next snapshot write because the encoder only emits `global` and
    // `by_room`.
    let config = parse_config_text(
        r#"
timeline:
  read_receipts:
    enabled: true
    global: false
"#,
    );

    assert_eq!(config.timeline.read_receipts.global, Some(false));
}

#[test]
fn fresh_profile_lands_on_openai_realtime_cloud_defaults() {
    // Brand new profile: nothing under integrations.transcription.* in YAML.
    // The parser should fill in conventional defaults so a user with just an
    // API key in the keychain reaches an `is_ready` resolver state.
    let config = parse_config_text("");

    assert_eq!(
        config
            .integrations
            .transcription
            .provider
            .as_ref()
            .map(|t| t.to_storage_string())
            .as_deref(),
        Some("openai_realtime")
    );
    assert_eq!(
        config.integrations.transcription.api_url.as_deref(),
        Some("https://api.openai.com/v1")
    );
}

#[test]
fn explicitly_empty_api_url_is_preserved() {
    // After the user picks Hosting → Other, `setHostingType` clears the URL
    // by writing an empty string. That state must survive the round-trip
    // (resolver should report "not ready" until they type a custom URL),
    // not silently snap back to OpenAI cloud.
    let config = parse_config_text(
        r#"
integrations:
  transcription:
    provider: openai_batch
    api_url: ''
"#,
    );

    assert_eq!(
        config
            .integrations
            .transcription
            .provider
            .as_ref()
            .map(|t| t.to_storage_string())
            .as_deref(),
        Some("openai_batch")
    );
    assert_eq!(config.integrations.transcription.api_url.as_deref(), Some(""));
}

#[test]
fn parses_integrations_transcription_by_room_partial_overrides() {
    let config = parse_config_text(
        r#"
integrations:
  transcription:
    by_room:
      "!example:matrix.org":
        model: gpt-4o-mini-transcribe
      "!other:matrix.org":
        api_url: "http://localhost:8080/v1"
        language: ""
"#,
    );

    let example = config
        .integrations
        .transcription
        .by_room
        .get("!example:matrix.org")
        .expect("entry for !example:matrix.org present");
    assert_eq!(example.model.as_deref(), Some("gpt-4o-mini-transcribe"));
    assert!(example.provider.is_none());
    assert!(example.api_url.is_none());
    assert!(example.language.is_none());
    assert!(example.prompt.is_none());

    let other = config
        .integrations
        .transcription
        .by_room
        .get("!other:matrix.org")
        .expect("entry for !other:matrix.org present");
    assert_eq!(other.api_url.as_deref(), Some("http://localhost:8080/v1"));
    // Empty-string language is a meaningful override (autodetect).
    assert_eq!(other.language.as_deref(), Some(""));
    assert!(other.model.is_none());
}

#[test]
fn encode_config_yaml_round_trips_partial_transcription_overrides() {
    let snapshot = SettingsConfigSnapshot {
        ui: SettingsConfigUiSection {
            scale_factor: 1.0,
            theme_slug: String::new(),
            theme_mode: String::new(),
            font_size_pt: 11.0,
            font_family: String::new(),
            font_emoji_family: String::new(),
            motion_animations_enabled: true,
            layout_density: "regular".to_owned(),
            avatars_circular: true,
            scrollbar_policy: "when_needed".to_owned(),
            default_avatar_style: "boring_avatars_bauhaus".to_owned(),
            language: String::new(),
        },
        navigation: SettingsConfigNavigationSection {
            room_list: SettingsConfigNavigationRoomListSection {
                show_last_message_time: true,
                last_message_preview: "long".to_owned(),
                show_unread_indicators: true,
                sort: "default".to_owned(),
                opening_policy: "open_in_new_tab".to_owned(),
            },
            communities: SettingsConfigNavigationCommunitiesSection {
                show_unread_indicators: true,
                filter_favourites: true,
                filter_people: true,
                filter_bots: true,
                filter_groups: true,
                filter_server_notices: true,
                filter_low_priority: true,
            },
            tabs: SettingsConfigNavigationTabsSection {
                auto_hide_with_single_tab: false,
                show_pin_button: "auto".to_owned(),
                pinned_tab_label: "avatar_only".to_owned(),
                tab_label: "avatar_and_label".to_owned(),
                preferred_width_px: 0,
                minimum_width_px: 0,
                max_recently_closed_timelines: 0,
            },
        },
        timeline: SettingsConfigTimelineSection {
            messages: SettingsConfigTimelineMessagesSection {
                style: "bubbles".to_owned(),
                layout_positioning: "opposing_by_sender".to_owned(),
                user_color_coding_policy: "adaptive_by_room_size".to_owned(),
                layout_avatar_size: "regular".to_owned(),
                layout_show_own_avatar: true,
                layout_max_width_percent: 70,
                layout_adaptive_positioning_breakpoint_px: 2000,
                sender_username: "only_in_large_rooms".to_owned(),
                emoji_only_enlarge: true,
                hover_highlight: true,
                drag_select: true,
            },
            formatted: SettingsConfigTimelineFormattedSection {
                code_syntax_highlighting: true,
            },
            typing: SettingsConfigTimelineTypingSection { show_enabled: true },
            read_receipts: SettingsConfigTimelineReadReceiptsSection {
                global: true,
                by_room: vec![],
            },
            message_actions: SettingsConfigTimelineMessageActionsSection {
                activation_policy: "actions_button".to_owned(),
                pinned_reactions: "👍".to_owned(),
            },
            media: SettingsConfigTimelineMediaSection {
                effects_enabled: true,
                animate_on_hover: true,
                image_display: "always".to_owned(),
                open_images_external: false,
                open_videos_external: false,
                autoplay_gif_videos: true,
                open_audio_external: false,
                default_audio_playback_speed: 1.0,
            },
            hidden_events: SettingsConfigTimelineHiddenEventsSection {
                global: vec![],
                by_room: vec![],
            },
            threads: SettingsConfigTimelineThreadsSection {
                collapse_replies_global: false,
                collapse_replies_by_room: vec![],
            },
            date_dividers: SettingsConfigTimelineDateDividersSection {
                enabled: true,
            },
            room_header: SettingsConfigTimelineRoomHeaderSection {
                button_labels: "adaptive".to_owned(),
            },
        },
        secrets: SettingsConfigSecretsSection {
            provider: "secret_service".to_owned(),
        },
        desktop: SettingsConfigDesktopSection {
            notifications: SettingsConfigDesktopNotificationsSection {
                enabled: true,
                attention_on_incoming: true,
                message_content_policy: "whenever_available".to_owned(),
            },
            attention: SettingsConfigDesktopAttentionSection {
                window_title: SettingsConfigDesktopAttentionWindowTitleSection {
                    enabled: true,
                },
                app_badge: SettingsConfigDesktopAttentionAppBadgeSection { enabled: true },
            },
            system_tray: SettingsConfigDesktopSystemTraySection {
                enabled: false,
                autostart: false,
                icon_style: "colorized".to_owned(),
            },
            window_focus_blur: SettingsConfigDesktopWindowFocusBlurSection {
                enabled: false,
                delay_seconds: 0,
            },
        },
        calls: crate::ffi::SettingsConfigCallsSection {
            legacy: crate::ffi::SettingsConfigCallsLegacySection { enabled: false },
            element: crate::ffi::SettingsConfigCallsElementSection { enabled: true },
            relay: crate::ffi::SettingsConfigCallsRelaySection {
                use_fallback_server: true,
            },
            devices: crate::ffi::SettingsConfigCallsDevicesSection {
                microphone: String::new(),
                camera: String::new(),
                camera_resolution: String::new(),
                camera_frame_rate: String::new(),
            },
            audio: crate::ffi::SettingsConfigCallsAudioSection {
                ringtone: String::new(),
            },
            screenshare: crate::ffi::SettingsConfigCallsScreenshareSection {
                frame_rate: 0,
                picture_in_picture: true,
                include_remote_video: true,
                show_cursor: true,
            },
        },
        network: SettingsConfigNetworkSection {
            encryption: SettingsConfigNetworkEncryptionSection {
                only_verified_users: false,
                share_with_trusted: true,
                key_backup: true,
            },
            presence_status_policy: "automatic_presence".to_owned(),
            tls_enable_certificate_validation: true,
            mrs_enabled: true,
            mrs_server_name: String::new(),
            http3_enabled: false,
        },
        integrations: SettingsConfigIntegrationsSection {
            dbus_api_access: "none".to_owned(),
            browser_command: String::new(),
            transcription_provider: "openai_batch".to_owned(),
            transcription_api_url: "https://api.openai.com/v1".to_owned(),
            transcription_model: "whisper-1".to_owned(),
            transcription_language: String::new(),
            transcription_prompt: String::new(),
            transcription_by_room: vec![
                SettingsConfigTranscriptionByRoomEntry {
                    key: "!example:matrix.org".to_owned(),
                    has_provider: false,
                    provider: String::new(),
                    has_api_url: false,
                    api_url: String::new(),
                    has_model: true,
                    model: "gpt-4o-mini-transcribe".to_owned(),
                    has_language: false,
                    language: String::new(),
                    has_prompt: false,
                    prompt: String::new(),
                },
                SettingsConfigTranscriptionByRoomEntry {
                    key: "!other:matrix.org".to_owned(),
                    has_provider: true,
                    provider: "openai_realtime".to_owned(),
                    has_api_url: true,
                    api_url: "http://localhost:8080/v1".to_owned(),
                    has_model: false,
                    model: String::new(),
                    has_language: true,
                    language: String::new(),
                    has_prompt: false,
                    prompt: String::new(),
                },
                SettingsConfigTranscriptionByRoomEntry {
                    key: "!empty:matrix.org".to_owned(),
                    has_provider: false,
                    provider: String::new(),
                    has_api_url: false,
                    api_url: String::new(),
                    has_model: false,
                    model: String::new(),
                    has_language: false,
                    language: String::new(),
                    has_prompt: false,
                    prompt: String::new(),
                },
            ],
        },
        composer: SettingsConfigComposerSection {
            input_markdown_to_html_enabled: true,
            input_send_key: "enter".to_owned(),
            input_auto_replace_emoji: "always".to_owned(),
            input_emoji_preferred_gender: "no_preference".to_owned(),
            input_emoji_preferred_skin_tone: "no_preference".to_owned(),
            input_inline_emoji_picker_enabled: true,
            input_inline_room_picker_enabled: true,
            input_inline_user_picker_enabled: true,
            input_selection_formatting_toolbar_enabled: true,
            input_transcription_enabled: true,
            input_spellcheck_enabled: true,
            input_spellcheck_languages: vec![],
            attachments_strip_image_metadata: true,
            typing_send_global: true,
            typing_send_by_room: vec![],
        },
    };

    let yaml = encode_config_yaml(&snapshot);
    let parsed = parse_config_text(&yaml);

    let example = parsed
        .integrations
        .transcription
        .by_room
        .get("!example:matrix.org")
        .expect("partial override survives round-trip");
    assert_eq!(example.model.as_deref(), Some("gpt-4o-mini-transcribe"));
    assert!(example.provider.is_none());
    assert!(example.api_url.is_none());
    assert!(example.language.is_none());
    assert!(example.prompt.is_none());

    let other = parsed
        .integrations
        .transcription
        .by_room
        .get("!other:matrix.org")
        .expect("entry with mixed overrides survives round-trip");
    assert!(other.provider.is_some());
    assert_eq!(other.api_url.as_deref(), Some("http://localhost:8080/v1"));
    assert_eq!(other.language.as_deref(), Some(""));
    assert!(other.model.is_none());
    assert!(other.prompt.is_none());

    // Rooms with no overrides at all are dropped from the YAML output.
    assert!(parsed
        .integrations
        .transcription
        .by_room
        .get("!empty:matrix.org")
        .is_none());
}

#[test]
fn encode_config_yaml_preserves_globals_when_by_room_empty() {
    // Mimics a "user has globals configured, no per-room overrides yet" state.
    let snapshot = SettingsConfigSnapshot {
        ui: SettingsConfigUiSection {
            scale_factor: 1.0,
            theme_slug: String::new(),
            theme_mode: String::new(),
            font_size_pt: 11.0,
            font_family: String::new(),
            font_emoji_family: String::new(),
            motion_animations_enabled: true,
            layout_density: "regular".to_owned(),
            avatars_circular: true,
            scrollbar_policy: "when_needed".to_owned(),
            default_avatar_style: "boring_avatars_bauhaus".to_owned(),
            language: String::new(),
        },
        navigation: SettingsConfigNavigationSection {
            room_list: SettingsConfigNavigationRoomListSection {
                show_last_message_time: true,
                last_message_preview: "long".to_owned(),
                show_unread_indicators: true,
                sort: "default".to_owned(),
                opening_policy: "open_in_new_tab".to_owned(),
            },
            communities: SettingsConfigNavigationCommunitiesSection {
                show_unread_indicators: true,
                filter_favourites: true,
                filter_people: true,
                filter_bots: true,
                filter_groups: true,
                filter_server_notices: true,
                filter_low_priority: true,
            },
            tabs: SettingsConfigNavigationTabsSection {
                auto_hide_with_single_tab: false,
                show_pin_button: "auto".to_owned(),
                pinned_tab_label: "avatar_only".to_owned(),
                tab_label: "avatar_and_label".to_owned(),
                preferred_width_px: 0,
                minimum_width_px: 0,
                max_recently_closed_timelines: 0,
            },
        },
        timeline: SettingsConfigTimelineSection {
            messages: SettingsConfigTimelineMessagesSection {
                style: "bubbles".to_owned(),
                layout_positioning: "opposing_by_sender".to_owned(),
                user_color_coding_policy: "adaptive_by_room_size".to_owned(),
                layout_avatar_size: "regular".to_owned(),
                layout_show_own_avatar: true,
                layout_max_width_percent: 70,
                layout_adaptive_positioning_breakpoint_px: 2000,
                sender_username: "only_in_large_rooms".to_owned(),
                emoji_only_enlarge: true,
                hover_highlight: true,
                drag_select: true,
            },
            formatted: SettingsConfigTimelineFormattedSection { code_syntax_highlighting: true },
            typing: SettingsConfigTimelineTypingSection { show_enabled: true },
            read_receipts: SettingsConfigTimelineReadReceiptsSection {
                global: true,
                by_room: vec![],
            },
            message_actions: SettingsConfigTimelineMessageActionsSection {
                activation_policy: "actions_button".to_owned(),
                pinned_reactions: "👍".to_owned(),
            },
            media: SettingsConfigTimelineMediaSection {
                effects_enabled: true,
                animate_on_hover: true,
                image_display: "always".to_owned(),
                open_images_external: false,
                open_videos_external: false,
                autoplay_gif_videos: true,
                open_audio_external: false,
                default_audio_playback_speed: 1.0,
            },
            hidden_events: SettingsConfigTimelineHiddenEventsSection { global: vec![], by_room: vec![] },
            threads: SettingsConfigTimelineThreadsSection { collapse_replies_global: false, collapse_replies_by_room: vec![] },
            date_dividers: SettingsConfigTimelineDateDividersSection { enabled: true },
            room_header: SettingsConfigTimelineRoomHeaderSection { button_labels: "adaptive".to_owned() },
        },
        secrets: SettingsConfigSecretsSection { provider: "secret_service".to_owned() },
        desktop: SettingsConfigDesktopSection {
            notifications: SettingsConfigDesktopNotificationsSection {
                enabled: true, attention_on_incoming: true,
                message_content_policy: "whenever_available".to_owned(),
            },
            attention: SettingsConfigDesktopAttentionSection {
                window_title: SettingsConfigDesktopAttentionWindowTitleSection { enabled: true },
                app_badge: SettingsConfigDesktopAttentionAppBadgeSection { enabled: true },
            },
            system_tray: SettingsConfigDesktopSystemTraySection { enabled: false, autostart: false, icon_style: "colorized".to_owned() },
            window_focus_blur: SettingsConfigDesktopWindowFocusBlurSection { enabled: false, delay_seconds: 0 },
        },
        calls: crate::ffi::SettingsConfigCallsSection {
            legacy: crate::ffi::SettingsConfigCallsLegacySection { enabled: false },
            element: crate::ffi::SettingsConfigCallsElementSection { enabled: true },
            relay: crate::ffi::SettingsConfigCallsRelaySection { use_fallback_server: true },
            devices: crate::ffi::SettingsConfigCallsDevicesSection {
                microphone: String::new(), camera: String::new(),
                camera_resolution: String::new(), camera_frame_rate: String::new(),
            },
            audio: crate::ffi::SettingsConfigCallsAudioSection { ringtone: String::new() },
            screenshare: crate::ffi::SettingsConfigCallsScreenshareSection {
                frame_rate: 0, picture_in_picture: true, include_remote_video: true, show_cursor: true,
            },
        },
        network: SettingsConfigNetworkSection {
            encryption: SettingsConfigNetworkEncryptionSection {
                only_verified_users: false, share_with_trusted: true, key_backup: true,
            },
            presence_status_policy: "automatic_presence".to_owned(),
            tls_enable_certificate_validation: true,
            mrs_enabled: true,
            mrs_server_name: String::new(),
            http3_enabled: false,
        },
        integrations: SettingsConfigIntegrationsSection {
            dbus_api_access: "none".to_owned(),
            browser_command: String::new(),
            transcription_provider: "openai_batch".to_owned(),
            transcription_api_url: "https://api.openai.com/v1".to_owned(),
            transcription_model: "".to_owned(),
            transcription_language: "".to_owned(),
            transcription_prompt: "".to_owned(),
            transcription_by_room: vec![],
        },
        composer: SettingsConfigComposerSection {
            input_markdown_to_html_enabled: true,
            input_send_key: "enter".to_owned(),
            input_auto_replace_emoji: "always".to_owned(),
            input_emoji_preferred_gender: "no_preference".to_owned(),
            input_emoji_preferred_skin_tone: "no_preference".to_owned(),
            input_inline_emoji_picker_enabled: true,
            input_inline_room_picker_enabled: true,
            input_inline_user_picker_enabled: true,
            input_selection_formatting_toolbar_enabled: true,
            input_transcription_enabled: true,
            input_spellcheck_enabled: true,
            input_spellcheck_languages: vec![],
            attachments_strip_image_metadata: true,
            typing_send_global: true,
            typing_send_by_room: vec![],
        },
    };

    let yaml = encode_config_yaml(&snapshot);
    let parsed = parse_config_text(&yaml);
    assert_eq!(
        parsed.integrations.transcription.api_url.as_deref(),
        Some("https://api.openai.com/v1"),
        "api_url should round-trip when there are no per-room overrides;\nencoded YAML:\n{yaml}"
    );
    assert_eq!(
        parsed
            .integrations
            .transcription
            .provider
            .as_ref()
            .map(|t| t.to_storage_string())
            .as_deref(),
        Some("openai_batch")
    );
}
