// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, settings};
use settings::config::defaults;

pub(crate) fn settings_load_startup_snapshot_for_profile(profile_id: &str) -> ffi::SettingsStartupSnapshot {
    let config_path = settings::storage::config_file_path_for_profile(profile_id);
    let snapshot = settings::startup::snapshot_from_config_path(&config_path);

    ffi::SettingsStartupSnapshot {
        ui_scale_factor: snapshot.ui_scale_factor.unwrap_or(defaults::SCALE_FACTOR),
    }
}

fn load_config_overview(config_text: &str) -> ffi::SettingsConfigOverview {
    let config = settings::config::parse_config_text(config_text);

    ffi::SettingsConfigOverview {
        ui_scale_factor: config.ui.scale.factor.unwrap_or(defaults::SCALE_FACTOR),
        theme_slug: config.ui.theme.slug,
        uses_file_secrets_provider: config.secrets.provider.to_storage_string() == "file",
    }
}

pub(crate) fn settings_load_config_overview_for_profile(profile_id: &str) -> ffi::SettingsConfigOverview {
    let config_path = settings::storage::config_file_path_for_profile(profile_id);
    load_config_overview(&settings::storage::read_text_file(&config_path, "config"))
}

pub(crate) fn settings_load_profile_overview_for_profile(profile_id: &str) -> ffi::SettingsProfileOverview {
    let loaded = settings::profile::load_profile_snapshot_for_profile(profile_id, true);

    ffi::SettingsProfileOverview {
        theme_slug: loaded.config.config.ui.theme.slug,
        uses_file_secrets_provider: loaded.config.config.secrets.provider.to_storage_string()
            == "file",
        user_id: loaded.session.user_id,
        homeserver: loaded.session.homeserver,
    }
}

pub(crate) fn settings_encode_string_map_yaml(entries: &Vec<ffi::SettingsStringMapEntry>) -> String {
    settings::secrets::encode_string_map_yaml(entries.as_slice())
}

pub(crate) fn settings_decode_string_map_yaml(serialized: &str) -> Vec<ffi::SettingsStringMapEntry> {
    settings::secrets::decode_string_map_yaml(serialized)
}

pub(crate) fn settings_encode_persisted_secrets_map_yaml(
    access_token: &str,
    entries: &Vec<ffi::SettingsStringMapEntry>,
) -> String {
    settings::secrets::encode_persisted_secrets_map_yaml(access_token, entries.as_slice())
}

pub(crate) fn settings_decode_persisted_secrets_map_yaml(
    serialized: &str,
) -> ffi::SettingsSecretsPayload {
    settings::secrets::decode_persisted_secrets_map_yaml(serialized)
}

pub(crate) fn settings_encode_named_string_map_yaml(
    root_key: &str,
    entries: &Vec<ffi::SettingsStringMapEntry>,
) -> String {
    settings::secrets::encode_named_string_map_yaml(root_key, entries.as_slice())
}

pub(crate) fn settings_decode_named_string_map_yaml(
    serialized: &str,
    root_key: &str,
) -> Vec<ffi::SettingsStringMapEntry> {
    settings::secrets::decode_named_string_map_yaml(serialized, root_key)
}

pub(crate) fn settings_load_persisted_secrets_file_for_profile(
    profile_id: &str,
) -> ffi::SettingsSecretsPayload {
    settings::secrets::load_persisted_secrets_file_for_profile(profile_id)
}

pub(crate) fn settings_write_persisted_secrets_file_for_profile(
    profile_id: &str,
    access_token: &str,
    entries: &Vec<ffi::SettingsStringMapEntry>,
    owner_read_write_only: bool,
) -> bool {
    settings::secrets::write_persisted_secrets_file_for_profile(
        profile_id,
        access_token,
        entries.as_slice(),
        owner_read_write_only,
    )
}

pub(crate) fn settings_remove_persisted_secrets_file_for_profile(profile_id: &str) -> bool {
    settings::secrets::remove_persisted_secrets_file_for_profile(profile_id)
}

pub(crate) fn settings_load_matrix_sdk_secrets_for_profile(
    profile_id: &str,
) -> Vec<ffi::SettingsStringMapEntry> {
    settings::secrets::load_matrix_sdk_secrets_for_profile(profile_id)
}

pub(crate) fn settings_write_matrix_sdk_secrets_for_profile(
    profile_id: &str,
    entries: &Vec<ffi::SettingsStringMapEntry>,
    owner_read_write_only: bool,
) -> bool {
    settings::secrets::write_matrix_sdk_secrets_for_profile(
        profile_id,
        entries.as_slice(),
        owner_read_write_only,
    )
}

pub(crate) fn settings_remove_matrix_sdk_secrets_file_for_profile(profile_id: &str) -> bool {
    settings::secrets::remove_matrix_sdk_secrets_file_for_profile(profile_id)
}

pub(crate) fn settings_load_persisted_matrix_session_secrets_for_profile(
    profile_id: &str,
) -> ffi::MatrixPersistedSessionSecrets {
    settings::secrets::load_persisted_matrix_session_secrets(profile_id)
}

pub(crate) fn settings_save_persisted_matrix_session_secrets_for_profile(
    profile_id: &str,
    store_passphrase: &str,
    homeserver_url: &str,
    serialized_session: &str,
) -> bool {
    settings::secrets::save_persisted_matrix_session_secrets(
        profile_id,
        store_passphrase,
        homeserver_url,
        serialized_session,
    )
}

pub(crate) fn settings_clear_persisted_matrix_session_secrets_for_profile(profile_id: &str) -> bool {
    settings::secrets::clear_persisted_matrix_session_secrets(profile_id)
}

pub(crate) fn ffi_config_ui_section(config: &settings::config::Config) -> ffi::SettingsConfigUiSection {
    ffi::SettingsConfigUiSection {
        scale_factor: config.ui.scale.factor.unwrap_or(defaults::SCALE_FACTOR),
        theme_slug: config.ui.theme.slug.clone(),
        theme_mode: config.ui.theme.mode.as_ref().map(|mode| mode.to_storage_string()).unwrap_or_default(),
        font_size_pt: config.ui.font.size_pt.unwrap_or(defaults::FONT_SIZE_PT),
        font_family: config.ui.font.family.clone(),
        font_emoji_family: config.ui.font.emoji_family.clone(),
        motion_animations_enabled: config.ui.motion.animations_enabled.unwrap_or(defaults::MOTION_ANIMATIONS_ENABLED),
        layout_density: config.ui.layout.density.to_storage_string(),
        avatars_circular: config.ui.avatars.circular.unwrap_or(defaults::AVATARS_CIRCULAR),
        scrollbar_policy: config.ui.scrollbar_policy.to_storage_string(),
        default_avatar_style: config.ui.avatars.default_avatar_style.to_storage_string(),
        language: config.ui.language.clone(),
    }
}

pub(crate) fn ffi_config_navigation_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigNavigationSection {
    ffi::SettingsConfigNavigationSection {
        room_list: ffi::SettingsConfigNavigationRoomListSection {
            show_last_message_time: config.navigation.room_list.show_last_message_time.unwrap_or(defaults::SHOW_LAST_MESSAGE_TIME),
            last_message_preview: config.navigation.room_list.last_message_preview.to_storage_string(),
            show_unread_indicators: config.navigation.room_list.show_unread_indicators.unwrap_or(defaults::SHOW_ROOM_LIST_UNREAD_INDICATORS),
            sort: config.navigation.room_list.sort.to_storage_string(),
            opening_policy: config.navigation.room_list.opening_policy.to_storage_string(),
        },
        communities: ffi::SettingsConfigNavigationCommunitiesSection {
            show_unread_indicators: config.navigation.communities.show_unread_indicators.unwrap_or(defaults::SHOW_COMMUNITIES_UNREAD_INDICATORS),
            filter_favourites: config.navigation.communities.filter_favourites.unwrap_or(defaults::COMMUNITIES_FILTER_FAVOURITES),
            filter_people: config.navigation.communities.filter_people.unwrap_or(defaults::COMMUNITIES_FILTER_PEOPLE),
            filter_bots: config.navigation.communities.filter_bots.unwrap_or(defaults::COMMUNITIES_FILTER_BOTS),
            filter_groups: config.navigation.communities.filter_groups.unwrap_or(defaults::COMMUNITIES_FILTER_GROUPS),
            filter_server_notices: config.navigation.communities.filter_server_notices.unwrap_or(defaults::COMMUNITIES_FILTER_SERVER_NOTICES),
            filter_low_priority: config.navigation.communities.filter_low_priority.unwrap_or(defaults::COMMUNITIES_FILTER_LOW_PRIORITY),
        },
        tabs: ffi::SettingsConfigNavigationTabsSection {
            auto_hide_with_single_tab: config.navigation.tabs.auto_hide_with_single_tab.unwrap_or(defaults::TABS_AUTO_HIDE_WITH_SINGLE_TAB),
            show_pin_button: config.navigation.tabs.show_pin_button.to_storage_string(),
            pinned_tab_label: config.navigation.tabs.pinned_tab_label.to_storage_string(),
            tab_label: config.navigation.tabs.tab_label.to_storage_string(),
            preferred_width_px: config.navigation.tabs.preferred_width_px.unwrap_or(defaults::TABS_PREFERRED_WIDTH_PX),
            minimum_width_px: config.navigation.tabs.minimum_width_px.unwrap_or(defaults::TABS_MINIMUM_WIDTH_PX),
            max_recently_closed_timelines: config.navigation.tabs.max_recently_closed_timelines.unwrap_or(defaults::TABS_MAX_RECENTLY_CLOSED_TIMELINES),
        },
    }
}

pub(crate) fn ffi_config_timeline_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigTimelineSection {
    let by_room = config
        .timeline
        .hidden_events
        .by_room
        .iter()
        .map(|(key, values)| ffi::SettingsStringListMapEntry {
            key: key.clone(),
            values: values.clone(),
        })
        .collect();

    ffi::SettingsConfigTimelineSection {
        messages: ffi::SettingsConfigTimelineMessagesSection {
            style: config.timeline.messages.style.to_storage_string(),
            layout_positioning: config.timeline.messages.layout.positioning.to_storage_string(),
            user_color_coding_policy: config.timeline.user_color_coding_policy.to_storage_string(),
            layout_avatar_size: config.timeline.messages.layout.avatar_size.to_storage_string(),
            layout_show_own_avatar: config.timeline.messages.layout.show_own_avatar.unwrap_or(defaults::LAYOUT_SHOW_OWN_AVATAR),
            layout_max_width_percent: config.timeline.messages.layout.max_width_percent.unwrap_or(defaults::LAYOUT_MAX_WIDTH_PERCENT),
            layout_adaptive_positioning_breakpoint_px: config.timeline.messages.layout.adaptive_positioning_breakpoint_px.unwrap_or(defaults::LAYOUT_ADAPTIVE_POSITIONING_BREAKPOINT_PX),
            sender_username: config.timeline.messages.sender_username.to_storage_string(),
            emoji_only_enlarge: config.timeline.messages.emoji_only_enlarge.unwrap_or(defaults::EMOJI_ONLY_ENLARGE),
            hover_highlight: config.timeline.messages.hover_highlight.unwrap_or(defaults::HOVER_HIGHLIGHT),
            drag_select: config.timeline.messages.drag_select.unwrap_or(defaults::DRAG_SELECT),
        },
        formatted: ffi::SettingsConfigTimelineFormattedSection {
            code_syntax_highlighting: config.timeline.formatted.code_syntax_highlighting.unwrap_or(defaults::CODE_SYNTAX_HIGHLIGHTING),
        },
        typing: ffi::SettingsConfigTimelineTypingSection {
            show_enabled: config.timeline.typing.show_enabled.unwrap_or(defaults::TYPING_SHOW_ENABLED),
        },
        read_receipts: ffi::SettingsConfigTimelineReadReceiptsSection {
            global: config
                .timeline
                .read_receipts
                .global
                .unwrap_or(defaults::READ_RECEIPTS_GLOBAL),
            by_room: config
                .timeline
                .read_receipts
                .by_room
                .iter()
                .map(|(room_id, value)| ffi::SettingsBoolMapEntry {
                    key: room_id.clone(),
                    value: *value,
                })
                .collect(),
        },
        message_actions: ffi::SettingsConfigTimelineMessageActionsSection {
            activation_policy: config.timeline.message_actions.activation_policy.to_storage_string(),
            pinned_reactions: config.timeline.message_actions.pinned_reactions.clone(),
        },
        media: ffi::SettingsConfigTimelineMediaSection {
            effects_enabled: config.timeline.media.effects_enabled.unwrap_or(defaults::MEDIA_EFFECTS_ENABLED),
            animate_on_hover: config.timeline.media.animate_on_hover.unwrap_or(defaults::MEDIA_ANIMATE_ON_HOVER),
            image_display: config.timeline.media.image_display.to_storage_string(),
            open_images_external: config.timeline.media.open_images_external.unwrap_or(defaults::MEDIA_OPEN_IMAGES_EXTERNAL),
            open_videos_external: config.timeline.media.open_videos_external.unwrap_or(defaults::MEDIA_OPEN_VIDEOS_EXTERNAL),
            autoplay_gif_videos: config.timeline.media.autoplay_gif_videos.unwrap_or(defaults::MEDIA_AUTOPLAY_GIF_VIDEOS),
            open_audio_external: config.timeline.media.open_audio_external.unwrap_or(defaults::MEDIA_OPEN_AUDIO_EXTERNAL),
            default_audio_playback_speed: config.timeline.media.default_audio_playback_speed.unwrap_or(defaults::MEDIA_DEFAULT_AUDIO_PLAYBACK_SPEED),
        },
        hidden_events: ffi::SettingsConfigTimelineHiddenEventsSection {
            global: config.timeline.hidden_events.global.clone().unwrap_or_else(|| {
                defaults::HIDDEN_TIMELINE_EVENT_TYPES.iter().map(|s| s.to_string()).collect()
            }),
            by_room,
        },
        threads: {
            let by_room: Vec<ffi::SettingsBoolMapEntry> = config
                .timeline
                .threads
                .collapse_replies
                .by_room
                .iter()
                .map(|(key, &value)| ffi::SettingsBoolMapEntry {
                    key: key.clone(),
                    value,
                })
                .collect();
            ffi::SettingsConfigTimelineThreadsSection {
                collapse_replies_global: config.timeline.threads.collapse_replies.global.unwrap_or(defaults::THREADS_COLLAPSE_REPLIES),
                collapse_replies_by_room: by_room,
            }
        },
        date_dividers: ffi::SettingsConfigTimelineDateDividersSection {
            enabled: config.timeline.date_dividers.enabled.unwrap_or(defaults::DATE_DIVIDERS_ENABLED),
        },
        room_header: ffi::SettingsConfigTimelineRoomHeaderSection {
            button_labels: config.timeline.room_header.button_labels.to_storage_string(),
        },
    }
}

pub(crate) fn ffi_config_secrets_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigSecretsSection {
    ffi::SettingsConfigSecretsSection {
        provider: config.secrets.provider.to_storage_string(),
    }
}

pub(crate) fn ffi_config_desktop_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigDesktopSection {
    ffi::SettingsConfigDesktopSection {
        notifications: ffi::SettingsConfigDesktopNotificationsSection {
            enabled: config.desktop.notifications.enabled.unwrap_or(defaults::NOTIFICATIONS_ENABLED),
            attention_on_incoming: config.desktop.notifications.attention_on_incoming.unwrap_or(defaults::NOTIFICATIONS_ATTENTION_ON_INCOMING),
            message_content_policy: config.desktop.notifications.message_content_policy.to_storage_string(),
        },
        attention: ffi::SettingsConfigDesktopAttentionSection {
            window_title: ffi::SettingsConfigDesktopAttentionWindowTitleSection {
                enabled: config.desktop.attention.window_title.enabled.unwrap_or(defaults::ATTENTION_WINDOW_TITLE_ENABLED),
            },
            app_badge: ffi::SettingsConfigDesktopAttentionAppBadgeSection {
                enabled: config.desktop.attention.app_badge.enabled.unwrap_or(defaults::ATTENTION_APP_BADGE_ENABLED),
            },
        },
        system_tray: ffi::SettingsConfigDesktopSystemTraySection {
            enabled: config.desktop.system_tray.enabled.unwrap_or(defaults::SYSTEM_TRAY_ENABLED),
            autostart: config.desktop.system_tray.autostart.unwrap_or(defaults::SYSTEM_TRAY_AUTOSTART),
            icon_style: config.desktop.system_tray.icon_style.to_storage_string(),
        },
        window_focus_blur: ffi::SettingsConfigDesktopWindowFocusBlurSection {
            enabled: config.desktop.window_focus_blur.enabled.unwrap_or(defaults::WINDOW_FOCUS_BLUR_ENABLED),
            delay_seconds: config.desktop.window_focus_blur.delay_seconds.unwrap_or(defaults::WINDOW_FOCUS_BLUR_DELAY_SECONDS),
        },
    }
}


pub(crate) fn ffi_config_calls_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigCallsSection {
    ffi::SettingsConfigCallsSection {
        legacy: ffi::SettingsConfigCallsLegacySection {
            enabled: config.calls.legacy.enabled.unwrap_or(defaults::CALLS_LEGACY_ENABLED),
        },
        element: ffi::SettingsConfigCallsElementSection {
            enabled: config.calls.element.enabled.unwrap_or(defaults::CALLS_ELEMENT_ENABLED),
        },
        relay: ffi::SettingsConfigCallsRelaySection {
            use_fallback_server: config.calls.relay.use_fallback_server.unwrap_or(defaults::CALLS_RELAY_USE_FALLBACK_SERVER),
        },
        devices: ffi::SettingsConfigCallsDevicesSection {
            microphone: config.calls.devices.microphone.clone(),
            camera: config.calls.devices.camera.clone(),
            camera_resolution: config.calls.devices.camera_resolution.clone(),
            camera_frame_rate: config.calls.devices.camera_frame_rate.clone(),
        },
        audio: ffi::SettingsConfigCallsAudioSection {
            ringtone: config.calls.audio.ringtone.clone(),
        },
        screenshare: ffi::SettingsConfigCallsScreenshareSection {
            frame_rate: config.calls.screenshare.frame_rate.unwrap_or(defaults::SCREENSHARE_FRAME_RATE),
            picture_in_picture: config.calls.screenshare.picture_in_picture.unwrap_or(defaults::SCREENSHARE_PICTURE_IN_PICTURE),
            include_remote_video: config.calls.screenshare.include_remote_video.unwrap_or(defaults::SCREENSHARE_INCLUDE_REMOTE_VIDEO),
            show_cursor: config.calls.screenshare.show_cursor.unwrap_or(defaults::SCREENSHARE_SHOW_CURSOR),
        },
    }
}

pub(crate) fn ffi_config_network_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigNetworkSection {
    ffi::SettingsConfigNetworkSection {
        encryption: ffi::SettingsConfigNetworkEncryptionSection {
            only_verified_users: config.network.encryption.only_verified_users.unwrap_or(defaults::ENCRYPTION_ONLY_VERIFIED_USERS),
            share_with_trusted: config.network.encryption.share_with_trusted.unwrap_or(defaults::ENCRYPTION_SHARE_WITH_TRUSTED),
            key_backup: config.network.encryption.key_backup.unwrap_or(defaults::ENCRYPTION_KEY_BACKUP),
        },
        presence_status_policy: config.network.presence_status_policy.to_storage_string(),
        tls_enable_certificate_validation: config.network.tls_enable_certificate_validation.unwrap_or(defaults::TLS_ENABLE_CERTIFICATE_VALIDATION),
        mrs_enabled: config.network.mrs_enabled.unwrap_or(defaults::MRS_ENABLED),
        mrs_server_name: config.network.mrs_server_name.clone(),
        http3_enabled: config.network.http3_enabled.unwrap_or(defaults::HTTP3_ENABLED),
    }
}

pub(crate) fn ffi_config_integrations_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigIntegrationsSection {
    ffi::SettingsConfigIntegrationsSection {
        dbus_api_access: config.integrations.dbus_api_access.to_storage_string(),
        browser_command: config.integrations.browser_command.clone(),
        transcription_provider: config
            .integrations
            .transcription
            .provider
            .as_ref()
            .map(|t| t.to_storage_string())
            .unwrap_or_default(),
        transcription_api_url: config
            .integrations
            .transcription
            .api_url
            .clone()
            .unwrap_or_default(),
        transcription_model: config
            .integrations
            .transcription
            .model
            .clone()
            .unwrap_or_default(),
        transcription_language: config
            .integrations
            .transcription
            .language
            .clone()
            .unwrap_or_default(),
        transcription_prompt: config
            .integrations
            .transcription
            .prompt
            .clone()
            .unwrap_or_default(),
        transcription_by_room: config
            .integrations
            .transcription
            .by_room
            .iter()
            .map(|(room_id, overrides)| {
                let (has_provider, provider) = match &overrides.provider {
                    Some(token) => (true, token.to_storage_string()),
                    None => (false, String::new()),
                };
                let (has_api_url, api_url) = match &overrides.api_url {
                    Some(value) => (true, value.clone()),
                    None => (false, String::new()),
                };
                let (has_model, model) = match &overrides.model {
                    Some(value) => (true, value.clone()),
                    None => (false, String::new()),
                };
                let (has_language, language) = match &overrides.language {
                    Some(value) => (true, value.clone()),
                    None => (false, String::new()),
                };
                let (has_prompt, prompt) = match &overrides.prompt {
                    Some(value) => (true, value.clone()),
                    None => (false, String::new()),
                };
                ffi::SettingsConfigTranscriptionByRoomEntry {
                    key: room_id.clone(),
                    has_provider,
                    provider,
                    has_api_url,
                    api_url,
                    has_model,
                    model,
                    has_language,
                    language,
                    has_prompt,
                    prompt,
                }
            })
            .collect(),
    }
}

pub(crate) fn ffi_config_composer_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigComposerSection {
    ffi::SettingsConfigComposerSection {
        input_markdown_to_html_enabled: config.composer.input_markdown_to_html_enabled.unwrap_or(defaults::INPUT_MARKDOWN_TO_HTML_ENABLED),
        input_send_key: config.composer.input_send_key.to_storage_string(),
        input_auto_replace_emoji: config.composer.input_auto_replace_emoji.to_storage_string(),
        input_emoji_preferred_gender: config.composer.input_emoji_preferred_gender.to_storage_string(),
        input_emoji_preferred_skin_tone: config.composer.input_emoji_preferred_skin_tone.to_storage_string(),
        input_inline_emoji_picker_enabled: config.composer.input_inline_emoji_picker_enabled.unwrap_or(defaults::INPUT_INLINE_EMOJI_PICKER_ENABLED),
        input_inline_room_picker_enabled: config.composer.input_inline_room_picker_enabled.unwrap_or(defaults::INPUT_INLINE_ROOM_PICKER_ENABLED),
        input_inline_user_picker_enabled: config.composer.input_inline_user_picker_enabled.unwrap_or(defaults::INPUT_INLINE_USER_PICKER_ENABLED),
        input_selection_formatting_toolbar_enabled: config.composer.input_selection_formatting_toolbar_enabled.unwrap_or(defaults::INPUT_SELECTION_FORMATTING_TOOLBAR_ENABLED),
        input_transcription_enabled: config.composer.input_transcription_enabled.unwrap_or(defaults::INPUT_TRANSCRIPTION_ENABLED),
        input_spellcheck_enabled: config.composer.input_spellcheck_enabled.unwrap_or(defaults::INPUT_SPELLCHECK_ENABLED),
        input_spellcheck_languages: config.composer.input_spellcheck_languages.clone().unwrap_or_default(),
        attachments_strip_image_metadata: config.composer.attachments_strip_image_metadata.unwrap_or(defaults::ATTACHMENTS_STRIP_IMAGE_METADATA),
        typing_send_global: config
            .composer
            .typing_send
            .global
            .unwrap_or(defaults::TYPING_SEND_GLOBAL),
        typing_send_by_room: config
            .composer
            .typing_send
            .by_room
            .iter()
            .map(|(room_id, value)| ffi::SettingsBoolMapEntry {
                key: room_id.clone(),
                value: *value,
            })
            .collect(),
    }
}

pub(crate) fn ffi_loaded_config(snapshot: settings::config::LoadedConfig) -> ffi::SettingsLoadedConfig {
    ffi::SettingsLoadedConfig {
        ui: ffi_config_ui_section(&snapshot.config),
        navigation: ffi_config_navigation_section(&snapshot.config),
        timeline: ffi_config_timeline_section(&snapshot.config),
        secrets: ffi_config_secrets_section(&snapshot.config),
        desktop: ffi_config_desktop_section(&snapshot.config),
        calls: ffi_config_calls_section(&snapshot.config),
        network: ffi_config_network_section(&snapshot.config),
        integrations: ffi_config_integrations_section(&snapshot.config),
        composer: ffi_config_composer_section(&snapshot.config),
        source_exists: snapshot.source_exists,
        source_version: snapshot.source_version,
        migrated_version: snapshot.migrated_version,
        had_future_version: snapshot.had_future_version,
        had_unsupported_path: snapshot.had_unsupported_path,
        should_write_back: snapshot.should_write_back,
        serialized_yaml: snapshot.serialized_yaml,
    }
}

pub(crate) fn ffi_loaded_session(snapshot: settings::session::LoadedSession) -> ffi::SettingsLoadedSession {
    ffi::SettingsLoadedSession {
        user_id: snapshot.user_id,
        device_id: snapshot.device_id,
        homeserver: snapshot.homeserver,
        source_exists: snapshot.source_exists,
        source_version: snapshot.source_version,
        migrated_version: snapshot.migrated_version,
        had_future_version: snapshot.had_future_version,
        had_unsupported_path: snapshot.had_unsupported_path,
        should_write_back: snapshot.should_write_back,
        serialized_yaml: snapshot.serialized_yaml,
    }
}

pub(crate) fn ffi_loaded_state(snapshot: settings::state::LoadedState) -> ffi::SettingsLoadedState {
    ffi::SettingsLoadedState {
        window_width: snapshot.window_width,
        window_height: snapshot.window_height,
        navigation_room_list_width_px: snapshot.navigation_room_list_width_px,
        navigation_communities_width_px: snapshot.navigation_communities_width_px,
        current_filter_id: snapshot.current_filter_id,
        current_room_id: snapshot.current_room_id,
        global_excludes: snapshot.global_excludes,
        unread_indicators_hidden_filters: snapshot.unread_indicators_hidden_filters,
        hidden_pins: snapshot.hidden_pins,
        hidden_widgets: snapshot.hidden_widgets,
        collapsed_spaces: snapshot.collapsed_spaces,
        hidden_spaces: snapshot.hidden_spaces,
        open_tabs: snapshot.open_tabs,
        pinned_tabs: snapshot.pinned_tabs,
        composer_drafts_by_room: snapshot.composer_drafts_by_room,
        sponsoring_status: snapshot.sponsoring_status,
        desktop_system_tray_first_close_prompted: snapshot
            .desktop_system_tray_first_close_prompted,
        source_exists: snapshot.source_exists,
        source_version: snapshot.source_version,
        migrated_version: snapshot.migrated_version,
        had_future_version: snapshot.had_future_version,
        had_unsupported_path: snapshot.had_unsupported_path,
        should_write_back: snapshot.should_write_back,
        serialized_yaml: snapshot.serialized_yaml,
    }
}

pub(crate) fn settings_load_config_snapshot(config_text: &str) -> ffi::SettingsLoadedConfig {
    ffi_loaded_config(settings::config::load_config_snapshot(config_text))
}

pub(crate) fn settings_open_profile_handle_for_profile(
    profile_id: &str,
    include_session: bool,
) -> Box<settings::profile::SettingsProfileHandle> {
    Box::new(settings::profile::SettingsProfileHandle::load_for_profile(
        profile_id,
        include_session,
    ))
}

pub(crate) fn settings_profile_snapshot(
    handle: &settings::profile::SettingsProfileHandle,
) -> ffi::SettingsLoadedProfile {
    handle.snapshot()
}

pub(crate) fn settings_profile_prepare_for_load(
    handle: std::pin::Pin<&mut settings::profile::SettingsProfileHandle>,
    full_load: bool,
    secure_backend_available: bool,
) {
    handle.prepare_for_load(full_load, secure_backend_available);
}

pub(crate) fn settings_profile_replace_config_snapshot(
    handle: std::pin::Pin<&mut settings::profile::SettingsProfileHandle>,
    snapshot: &ffi::SettingsConfigSnapshot,
) {
    handle.replace_config_snapshot(snapshot);
}

pub(crate) fn settings_profile_replace_session_identity(
    handle: std::pin::Pin<&mut settings::profile::SettingsProfileHandle>,
    user_id: &str,
    homeserver: &str,
    device_id: &str,
) {
    handle.replace_session_identity(user_id, homeserver, device_id);
}

pub(crate) fn settings_profile_replace_state_snapshot(
    handle: std::pin::Pin<&mut settings::profile::SettingsProfileHandle>,
    snapshot: &ffi::SettingsStateSnapshot,
) {
    handle.replace_state_snapshot(snapshot);
}

pub(crate) fn settings_profile_replace_secrets_payload(
    handle: std::pin::Pin<&mut settings::profile::SettingsProfileHandle>,
    access_token: &str,
    entries: &Vec<ffi::SettingsStringMapEntry>,
) {
    handle.replace_secrets_payload(access_token, entries.as_slice());
}

pub(crate) fn settings_profile_clear_secrets(
    handle: std::pin::Pin<&mut settings::profile::SettingsProfileHandle>,
) -> bool {
    handle.clear_secrets()
}

pub(crate) fn settings_profile_clear_auth(
    handle: std::pin::Pin<&mut settings::profile::SettingsProfileHandle>,
) -> bool {
    handle.clear_auth()
}

pub(crate) fn settings_profile_flush(
    handle: std::pin::Pin<&mut settings::profile::SettingsProfileHandle>,
    write_config: bool,
    write_session: bool,
    write_secrets: bool,
    write_state: bool,
) -> ffi::SettingsProfileFlushResult {
    handle.flush(write_config, write_session, write_secrets, write_state)
}

pub(crate) fn settings_load_session_snapshot(session_text: &str) -> ffi::SettingsLoadedSession {
    ffi_loaded_session(settings::session::load_session_snapshot(session_text))
}

pub(crate) fn settings_load_state_snapshot(state_text: &str) -> ffi::SettingsLoadedState {
    ffi_loaded_state(settings::state::load_state_snapshot(state_text))
}

fn clone_string_list_map_entries(
    entries: &Vec<ffi::SettingsStringListMapEntry>,
) -> Vec<ffi::SettingsStringListMapEntry> {
    entries
        .iter()
        .map(|entry| ffi::SettingsStringListMapEntry {
            key: entry.key.clone(),
            values: entry.values.iter().map(|value| value.clone()).collect(),
        })
        .collect()
}

fn clone_config_ui_section(section: &ffi::SettingsConfigUiSection) -> ffi::SettingsConfigUiSection {
    ffi::SettingsConfigUiSection {
        scale_factor: section.scale_factor,
        theme_slug: section.theme_slug.clone(),
        theme_mode: section.theme_mode.clone(),
        font_size_pt: section.font_size_pt,
        font_family: section.font_family.clone(),
        font_emoji_family: section.font_emoji_family.clone(),
        motion_animations_enabled: section.motion_animations_enabled,
        layout_density: section.layout_density.clone(),
        avatars_circular: section.avatars_circular,
        scrollbar_policy: section.scrollbar_policy.clone(),
        default_avatar_style: section.default_avatar_style.clone(),
        language: section.language.clone(),
    }
}

fn clone_config_navigation_section(
    section: &ffi::SettingsConfigNavigationSection,
) -> ffi::SettingsConfigNavigationSection {
    ffi::SettingsConfigNavigationSection {
        room_list: ffi::SettingsConfigNavigationRoomListSection {
            show_last_message_time: section.room_list.show_last_message_time,
            last_message_preview: section.room_list.last_message_preview.clone(),
            show_unread_indicators: section.room_list.show_unread_indicators,
            sort: section.room_list.sort.clone(),
            opening_policy: section.room_list.opening_policy.clone(),
        },
        communities: ffi::SettingsConfigNavigationCommunitiesSection {
            show_unread_indicators: section.communities.show_unread_indicators,
            filter_favourites: section.communities.filter_favourites,
            filter_people: section.communities.filter_people,
            filter_bots: section.communities.filter_bots,
            filter_groups: section.communities.filter_groups,
            filter_server_notices: section.communities.filter_server_notices,
            filter_low_priority: section.communities.filter_low_priority,
        },
        tabs: ffi::SettingsConfigNavigationTabsSection {
            auto_hide_with_single_tab: section.tabs.auto_hide_with_single_tab,
            show_pin_button: section.tabs.show_pin_button.clone(),
            pinned_tab_label: section.tabs.pinned_tab_label.clone(),
            tab_label: section.tabs.tab_label.clone(),
            preferred_width_px: section.tabs.preferred_width_px,
            minimum_width_px: section.tabs.minimum_width_px,
            max_recently_closed_timelines: section.tabs.max_recently_closed_timelines,
        },
    }
}

fn clone_config_timeline_section(
    section: &ffi::SettingsConfigTimelineSection,
) -> ffi::SettingsConfigTimelineSection {
    ffi::SettingsConfigTimelineSection {
        messages: ffi::SettingsConfigTimelineMessagesSection {
            style: section.messages.style.clone(),
            layout_positioning: section.messages.layout_positioning.clone(),
            user_color_coding_policy: section.messages.user_color_coding_policy.clone(),
            layout_avatar_size: section.messages.layout_avatar_size.clone(),
            layout_show_own_avatar: section.messages.layout_show_own_avatar,
            layout_max_width_percent: section.messages.layout_max_width_percent,
            layout_adaptive_positioning_breakpoint_px: section
                .messages
                .layout_adaptive_positioning_breakpoint_px,
            sender_username: section.messages.sender_username.clone(),
            emoji_only_enlarge: section.messages.emoji_only_enlarge,
            hover_highlight: section.messages.hover_highlight,
            drag_select: section.messages.drag_select,
        },
        formatted: ffi::SettingsConfigTimelineFormattedSection {
            code_syntax_highlighting: section.formatted.code_syntax_highlighting,
        },
        typing: ffi::SettingsConfigTimelineTypingSection {
            show_enabled: section.typing.show_enabled,
        },
        read_receipts: ffi::SettingsConfigTimelineReadReceiptsSection {
            global: section.read_receipts.global,
            by_room: section
                .read_receipts
                .by_room
                .iter()
                .map(|entry| ffi::SettingsBoolMapEntry {
                    key: entry.key.clone(),
                    value: entry.value,
                })
                .collect(),
        },
        message_actions: ffi::SettingsConfigTimelineMessageActionsSection {
            activation_policy: section.message_actions.activation_policy.clone(),
            pinned_reactions: section.message_actions.pinned_reactions.clone(),
        },
        media: ffi::SettingsConfigTimelineMediaSection {
            effects_enabled: section.media.effects_enabled,
            animate_on_hover: section.media.animate_on_hover,
            image_display: section.media.image_display.clone(),
            open_images_external: section.media.open_images_external,
            open_videos_external: section.media.open_videos_external,
            autoplay_gif_videos: section.media.autoplay_gif_videos,
            open_audio_external: section.media.open_audio_external,
            default_audio_playback_speed: section.media.default_audio_playback_speed,
        },
        hidden_events: ffi::SettingsConfigTimelineHiddenEventsSection {
            global: section.hidden_events.global.iter().map(|value| value.clone()).collect(),
            by_room: clone_string_list_map_entries(&section.hidden_events.by_room),
        },
        threads: ffi::SettingsConfigTimelineThreadsSection {
            collapse_replies_global: section.threads.collapse_replies_global,
            collapse_replies_by_room: section
                .threads
                .collapse_replies_by_room
                .iter()
                .map(|entry| ffi::SettingsBoolMapEntry {
                    key: entry.key.clone(),
                    value: entry.value,
                })
                .collect(),
        },
        date_dividers: ffi::SettingsConfigTimelineDateDividersSection {
            enabled: section.date_dividers.enabled,
        },
        room_header: ffi::SettingsConfigTimelineRoomHeaderSection {
            button_labels: section.room_header.button_labels.clone(),
        },
    }
}

fn clone_config_secrets_section(
    section: &ffi::SettingsConfigSecretsSection,
) -> ffi::SettingsConfigSecretsSection {
    ffi::SettingsConfigSecretsSection {
        provider: section.provider.clone(),
    }
}

fn clone_config_desktop_section(
    section: &ffi::SettingsConfigDesktopSection,
) -> ffi::SettingsConfigDesktopSection {
    ffi::SettingsConfigDesktopSection {
        notifications: ffi::SettingsConfigDesktopNotificationsSection {
            enabled: section.notifications.enabled,
            attention_on_incoming: section.notifications.attention_on_incoming,
            message_content_policy: section.notifications.message_content_policy.clone(),
        },
        attention: ffi::SettingsConfigDesktopAttentionSection {
            window_title: ffi::SettingsConfigDesktopAttentionWindowTitleSection {
                enabled: section.attention.window_title.enabled,
            },
            app_badge: ffi::SettingsConfigDesktopAttentionAppBadgeSection {
                enabled: section.attention.app_badge.enabled,
            },
        },
        system_tray: ffi::SettingsConfigDesktopSystemTraySection {
            enabled: section.system_tray.enabled,
            autostart: section.system_tray.autostart,
            icon_style: section.system_tray.icon_style.clone(),
        },
        window_focus_blur: ffi::SettingsConfigDesktopWindowFocusBlurSection {
            enabled: section.window_focus_blur.enabled,
            delay_seconds: section.window_focus_blur.delay_seconds,
        },
    }
}

fn clone_config_network_encryption_section(
    section: &ffi::SettingsConfigNetworkEncryptionSection,
) -> ffi::SettingsConfigNetworkEncryptionSection {
    ffi::SettingsConfigNetworkEncryptionSection {
        only_verified_users: section.only_verified_users,
        share_with_trusted: section.share_with_trusted,
        key_backup: section.key_backup,
    }
}

fn clone_config_calls_section(
    section: &ffi::SettingsConfigCallsSection,
) -> ffi::SettingsConfigCallsSection {
    ffi::SettingsConfigCallsSection {
        legacy: ffi::SettingsConfigCallsLegacySection {
            enabled: section.legacy.enabled,
        },
        element: ffi::SettingsConfigCallsElementSection {
            enabled: section.element.enabled,
        },
        relay: ffi::SettingsConfigCallsRelaySection {
            use_fallback_server: section.relay.use_fallback_server,
        },
        devices: ffi::SettingsConfigCallsDevicesSection {
            microphone: section.devices.microphone.clone(),
            camera: section.devices.camera.clone(),
            camera_resolution: section.devices.camera_resolution.clone(),
            camera_frame_rate: section.devices.camera_frame_rate.clone(),
        },
        audio: ffi::SettingsConfigCallsAudioSection {
            ringtone: section.audio.ringtone.clone(),
        },
        screenshare: ffi::SettingsConfigCallsScreenshareSection {
            frame_rate: section.screenshare.frame_rate,
            picture_in_picture: section.screenshare.picture_in_picture,
            include_remote_video: section.screenshare.include_remote_video,
            show_cursor: section.screenshare.show_cursor,
        },
    }
}

fn clone_config_network_section(
    section: &ffi::SettingsConfigNetworkSection,
) -> ffi::SettingsConfigNetworkSection {
    ffi::SettingsConfigNetworkSection {
        encryption: clone_config_network_encryption_section(&section.encryption),
        presence_status_policy: section.presence_status_policy.clone(),
        tls_enable_certificate_validation: section.tls_enable_certificate_validation,
        mrs_enabled: section.mrs_enabled,
        mrs_server_name: section.mrs_server_name.clone(),
        http3_enabled: section.http3_enabled,
    }
}

fn clone_config_integrations_section(
    section: &ffi::SettingsConfigIntegrationsSection,
) -> ffi::SettingsConfigIntegrationsSection {
    ffi::SettingsConfigIntegrationsSection {
        dbus_api_access: section.dbus_api_access.clone(),
        browser_command: section.browser_command.clone(),
        transcription_provider: section.transcription_provider.clone(),
        transcription_api_url: section.transcription_api_url.clone(),
        transcription_model: section.transcription_model.clone(),
        transcription_language: section.transcription_language.clone(),
        transcription_prompt: section.transcription_prompt.clone(),
        transcription_by_room: section
            .transcription_by_room
            .iter()
            .map(|entry| ffi::SettingsConfigTranscriptionByRoomEntry {
                key: entry.key.clone(),
                has_provider: entry.has_provider,
                provider: entry.provider.clone(),
                has_api_url: entry.has_api_url,
                api_url: entry.api_url.clone(),
                has_model: entry.has_model,
                model: entry.model.clone(),
                has_language: entry.has_language,
                language: entry.language.clone(),
                has_prompt: entry.has_prompt,
                prompt: entry.prompt.clone(),
            })
            .collect(),
    }
}

fn clone_config_composer_section(
    section: &ffi::SettingsConfigComposerSection,
) -> ffi::SettingsConfigComposerSection {
    ffi::SettingsConfigComposerSection {
        input_markdown_to_html_enabled: section.input_markdown_to_html_enabled,
        input_send_key: section.input_send_key.clone(),
        input_auto_replace_emoji: section.input_auto_replace_emoji.clone(),
        input_emoji_preferred_gender: section.input_emoji_preferred_gender.clone(),
        input_emoji_preferred_skin_tone: section.input_emoji_preferred_skin_tone.clone(),
        input_inline_emoji_picker_enabled: section.input_inline_emoji_picker_enabled,
        input_inline_room_picker_enabled: section.input_inline_room_picker_enabled,
        input_inline_user_picker_enabled: section.input_inline_user_picker_enabled,
        input_selection_formatting_toolbar_enabled: section.input_selection_formatting_toolbar_enabled,
        input_transcription_enabled: section.input_transcription_enabled,
        input_spellcheck_enabled: section.input_spellcheck_enabled,
        input_spellcheck_languages: section.input_spellcheck_languages.iter().cloned().collect(),
        attachments_strip_image_metadata: section.attachments_strip_image_metadata,
        typing_send_global: section.typing_send_global,
        typing_send_by_room: section
            .typing_send_by_room
            .iter()
            .map(|entry| ffi::SettingsBoolMapEntry {
                key: entry.key.clone(),
                value: entry.value,
            })
            .collect(),
    }
}

pub(in crate::settings) fn loaded_config_to_snapshot(
    loaded: &ffi::SettingsLoadedConfig,
) -> ffi::SettingsConfigSnapshot {
    ffi::SettingsConfigSnapshot {
        ui: clone_config_ui_section(&loaded.ui),
        navigation: clone_config_navigation_section(&loaded.navigation),
        timeline: clone_config_timeline_section(&loaded.timeline),
        secrets: clone_config_secrets_section(&loaded.secrets),
        desktop: clone_config_desktop_section(&loaded.desktop),
        calls: clone_config_calls_section(&loaded.calls),
        network: clone_config_network_section(&loaded.network),
        integrations: clone_config_integrations_section(&loaded.integrations),
        composer: clone_config_composer_section(&loaded.composer),
    }
}

pub(in crate::settings) fn clone_loaded_config(
    loaded: &ffi::SettingsLoadedConfig,
) -> ffi::SettingsLoadedConfig {
    ffi::SettingsLoadedConfig {
        ui: clone_config_ui_section(&loaded.ui),
        navigation: clone_config_navigation_section(&loaded.navigation),
        timeline: clone_config_timeline_section(&loaded.timeline),
        secrets: clone_config_secrets_section(&loaded.secrets),
        desktop: clone_config_desktop_section(&loaded.desktop),
        calls: clone_config_calls_section(&loaded.calls),
        network: clone_config_network_section(&loaded.network),
        integrations: clone_config_integrations_section(&loaded.integrations),
        composer: clone_config_composer_section(&loaded.composer),
        source_exists: loaded.source_exists,
        source_version: loaded.source_version,
        migrated_version: loaded.migrated_version,
        had_future_version: loaded.had_future_version,
        had_unsupported_path: loaded.had_unsupported_path,
        should_write_back: loaded.should_write_back,
        serialized_yaml: loaded.serialized_yaml.clone(),
    }
}

pub(in crate::settings) fn loaded_config_from_snapshot(
    snapshot: &ffi::SettingsConfigSnapshot,
) -> ffi::SettingsLoadedConfig {
    ffi::SettingsLoadedConfig {
        ui: clone_config_ui_section(&snapshot.ui),
        navigation: clone_config_navigation_section(&snapshot.navigation),
        timeline: clone_config_timeline_section(&snapshot.timeline),
        secrets: clone_config_secrets_section(&snapshot.secrets),
        desktop: clone_config_desktop_section(&snapshot.desktop),
        calls: clone_config_calls_section(&snapshot.calls),
        network: clone_config_network_section(&snapshot.network),
        integrations: clone_config_integrations_section(&snapshot.integrations),
        composer: clone_config_composer_section(&snapshot.composer),
        source_exists: true,
        source_version: settings::config::CURRENT_CONFIG_SCHEMA_VERSION,
        migrated_version: settings::config::CURRENT_CONFIG_SCHEMA_VERSION,
        had_future_version: false,
        had_unsupported_path: false,
        should_write_back: false,
        serialized_yaml: settings::config::encode_config_yaml(snapshot),
    }
}

pub(in crate::settings) fn clone_loaded_session(
    loaded: &ffi::SettingsLoadedSession,
) -> ffi::SettingsLoadedSession {
    ffi::SettingsLoadedSession {
        user_id: loaded.user_id.clone(),
        device_id: loaded.device_id.clone(),
        homeserver: loaded.homeserver.clone(),
        source_exists: loaded.source_exists,
        source_version: loaded.source_version,
        migrated_version: loaded.migrated_version,
        had_future_version: loaded.had_future_version,
        had_unsupported_path: loaded.had_unsupported_path,
        should_write_back: loaded.should_write_back,
        serialized_yaml: loaded.serialized_yaml.clone(),
    }
}

pub(in crate::settings) fn loaded_session_from_identity(
    user_id: &str,
    homeserver: &str,
    device_id: &str,
) -> ffi::SettingsLoadedSession {
    ffi::SettingsLoadedSession {
        user_id: user_id.to_owned(),
        device_id: device_id.to_owned(),
        homeserver: homeserver.to_owned(),
        source_exists: true,
        source_version: settings::session::CURRENT_SESSION_SCHEMA_VERSION,
        migrated_version: settings::session::CURRENT_SESSION_SCHEMA_VERSION,
        had_future_version: false,
        had_unsupported_path: false,
        should_write_back: false,
        serialized_yaml: settings::session::encode_session_yaml(user_id, homeserver, device_id),
    }
}

pub(in crate::settings) fn clone_loaded_state(loaded: &ffi::SettingsLoadedState) -> ffi::SettingsLoadedState {
    ffi::SettingsLoadedState {
        window_width: loaded.window_width,
        window_height: loaded.window_height,
        navigation_room_list_width_px: loaded.navigation_room_list_width_px,
        navigation_communities_width_px: loaded.navigation_communities_width_px,
        current_filter_id: loaded.current_filter_id.clone(),
        current_room_id: loaded.current_room_id.clone(),
        global_excludes: loaded.global_excludes.iter().cloned().collect(),
        unread_indicators_hidden_filters: loaded.unread_indicators_hidden_filters.iter().cloned().collect(),
        hidden_pins: loaded.hidden_pins.iter().cloned().collect(),
        hidden_widgets: loaded.hidden_widgets.iter().cloned().collect(),
        collapsed_spaces: loaded.collapsed_spaces.iter().cloned().collect(),
        hidden_spaces: loaded.hidden_spaces.iter().cloned().collect(),
        open_tabs: loaded.open_tabs.iter().cloned().collect(),
        pinned_tabs: loaded.pinned_tabs.iter().cloned().collect(),
        composer_drafts_by_room: loaded
            .composer_drafts_by_room
            .iter()
            .map(|entry| ffi::SettingsStringMapEntry {
                key: entry.key.clone(),
                value: entry.value.clone(),
            })
            .collect(),
        sponsoring_status: loaded.sponsoring_status.clone(),
        desktop_system_tray_first_close_prompted: loaded.desktop_system_tray_first_close_prompted,
        source_exists: loaded.source_exists,
        source_version: loaded.source_version,
        migrated_version: loaded.migrated_version,
        had_future_version: loaded.had_future_version,
        had_unsupported_path: loaded.had_unsupported_path,
        should_write_back: loaded.should_write_back,
        serialized_yaml: loaded.serialized_yaml.clone(),
    }
}

pub(in crate::settings) fn loaded_state_from_snapshot(
    snapshot: &ffi::SettingsStateSnapshot,
) -> ffi::SettingsLoadedState {
    ffi::SettingsLoadedState {
        window_width: snapshot.window_width,
        window_height: snapshot.window_height,
        navigation_room_list_width_px: snapshot.navigation_room_list_width_px,
        navigation_communities_width_px: snapshot.navigation_communities_width_px,
        current_filter_id: snapshot.current_filter_id.clone(),
        current_room_id: snapshot.current_room_id.clone(),
        global_excludes: snapshot.global_excludes.iter().cloned().collect(),
        unread_indicators_hidden_filters: snapshot.unread_indicators_hidden_filters.iter().cloned().collect(),
        hidden_pins: snapshot.hidden_pins.iter().cloned().collect(),
        hidden_widgets: snapshot.hidden_widgets.iter().cloned().collect(),
        collapsed_spaces: snapshot.collapsed_spaces.iter().cloned().collect(),
        hidden_spaces: snapshot.hidden_spaces.iter().cloned().collect(),
        open_tabs: snapshot.open_tabs.iter().cloned().collect(),
        pinned_tabs: snapshot.pinned_tabs.iter().cloned().collect(),
        composer_drafts_by_room: snapshot
            .composer_drafts_by_room
            .iter()
            .map(|entry| ffi::SettingsStringMapEntry {
                key: entry.key.clone(),
                value: entry.value.clone(),
            })
            .collect(),
        sponsoring_status: snapshot.sponsoring_status.clone(),
        desktop_system_tray_first_close_prompted: snapshot.desktop_system_tray_first_close_prompted,
        source_exists: true,
        source_version: settings::state::CURRENT_STATE_SCHEMA_VERSION,
        migrated_version: settings::state::CURRENT_STATE_SCHEMA_VERSION,
        had_future_version: false,
        had_unsupported_path: false,
        should_write_back: false,
        serialized_yaml: settings::state::encode_state_yaml(snapshot),
    }
}

pub(in crate::settings) fn clone_loaded_profile(
    loaded: &ffi::SettingsLoadedProfile,
) -> ffi::SettingsLoadedProfile {
    ffi::SettingsLoadedProfile {
        config: clone_loaded_config(&loaded.config),
        session: clone_loaded_session(&loaded.session),
        state: clone_loaded_state(&loaded.state),
        secrets: ffi::SettingsSecretsPayload {
            access_token: loaded.secrets.access_token.clone(),
            secrets: loaded.secrets.secrets.to_vec(),
            had_stale_values: loaded.secrets.had_stale_values,
        },
        uses_file_secrets_provider: loaded.uses_file_secrets_provider,
        startup_secrets_provider_changed: loaded.startup_secrets_provider_changed,
        secrets_provider_fallback_warning_visible: loaded
            .secrets_provider_fallback_warning_visible,
    }
}

pub(in crate::settings) fn write_loaded_config_to_path(
    config_path: &str,
    loaded: &ffi::SettingsLoadedConfig,
) -> bool {
    settings::config::write_config_snapshot_to_path(config_path, &loaded_config_to_snapshot(loaded))
}

pub(in crate::settings) fn write_loaded_session_to_path(
    session_path: &str,
    loaded: &ffi::SettingsLoadedSession,
) -> bool {
    settings::storage::write_text_file(session_path, &loaded.serialized_yaml, false)
}

pub(in crate::settings) fn write_loaded_state_to_path(
    state_path: &str,
    loaded: &ffi::SettingsLoadedState,
) -> bool {
    settings::storage::write_text_file(state_path, &loaded.serialized_yaml, false)
}
