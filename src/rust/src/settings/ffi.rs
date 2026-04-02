// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, settings};

pub(crate) fn settings_load_startup_snapshot_for_profile(profile_id: &str) -> ffi::SettingsStartupSnapshot {
    let config_path = settings::storage::config_file_path_for_profile(profile_id);
    let snapshot = settings::startup::snapshot_from_config_path(&config_path);

    ffi::SettingsStartupSnapshot {
        has_ui_scale_factor: snapshot.ui_scale_factor.is_some(),
        ui_scale_factor: snapshot.ui_scale_factor.unwrap_or_default(),
    }
}

fn load_config_overview(config_text: &str) -> ffi::SettingsConfigOverview {
    let config = settings::config::parse_config_text(config_text);

    ffi::SettingsConfigOverview {
        has_ui_scale_factor: config.ui.scale.factor.is_some(),
        ui_scale_factor: config.ui.scale.factor.unwrap_or_default(),
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

pub(crate) fn ffi_config_ui_section(config: &settings::config::Config) -> ffi::SettingsConfigUiSection {
    ffi::SettingsConfigUiSection {
        has_scale_factor: config.ui.scale.factor.is_some(),
        scale_factor: config.ui.scale.factor.unwrap_or_default(),
        theme_slug: config.ui.theme.slug.clone(),
        has_font_size_pt: config.ui.font.size_pt.is_some(),
        font_size_pt: config.ui.font.size_pt.unwrap_or_default(),
        font_family: config.ui.font.family.clone(),
        font_emoji_family: config.ui.font.emoji_family.clone(),
        has_motion_animations_enabled: config.ui.motion.animations_enabled.is_some(),
        motion_animations_enabled: config.ui.motion.animations_enabled.unwrap_or_default(),
        input_mode: config.ui.input.mode.to_storage_string(),
        has_input_touch_swipe_gestures_enabled: config.ui.input.touch_swipe_gestures_enabled.is_some(),
        input_touch_swipe_gestures_enabled: config
            .ui
            .input
            .touch_swipe_gestures_enabled
            .unwrap_or_default(),
        has_layout_content_max_width_px: config.ui.layout.content_max_width_px.is_some(),
        layout_content_max_width_px: config.ui.layout.content_max_width_px.unwrap_or_default(),
        has_layout_compact_mode: config.ui.layout.compact_mode.is_some(),
        layout_compact_mode: config.ui.layout.compact_mode.unwrap_or_default(),
        has_avatars_circular: config.ui.avatars.circular.is_some(),
        avatars_circular: config.ui.avatars.circular.unwrap_or_default(),
        scrollbar_policy: config.ui.scrollbar_policy.to_storage_string(),
        default_avatar_style: config.ui.avatars.default_avatar_style.to_storage_string(),
    }
}

pub(crate) fn ffi_config_sidebars_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigSidebarsSection {
    ffi::SettingsConfigSidebarsSection {
        room_list: ffi::SettingsConfigSidebarsRoomListSection {
            has_show_last_message_time: config.sidebars.room_list.show_last_message_time.is_some(),
            show_last_message_time: config
                .sidebars
                .room_list
                .show_last_message_time
                .unwrap_or_default(),
            last_message_preview: config.sidebars.room_list.last_message_preview.to_storage_string(),
            has_show_community_counts: config.sidebars.room_list.show_community_counts.is_some(),
            show_community_counts: config
                .sidebars
                .room_list
                .show_community_counts
                .unwrap_or_default(),
            sort: config.sidebars.room_list.sort.to_storage_string(),
            unread_detection_policy: config
                .sidebars
                .room_list
                .unread_detection_policy
                .to_storage_string(),
        },
        communities: ffi::SettingsConfigSidebarsCommunitiesSection {
            has_visible: config.sidebars.communities.visible.is_some(),
            visible: config.sidebars.communities.visible.unwrap_or_default(),
            has_filter_favourites: config.sidebars.communities.filter_favourites.is_some(),
            filter_favourites: config
                .sidebars
                .communities
                .filter_favourites
                .unwrap_or_default(),
            has_filter_people: config.sidebars.communities.filter_people.is_some(),
            filter_people: config.sidebars.communities.filter_people.unwrap_or_default(),
            has_filter_bots: config.sidebars.communities.filter_bots.is_some(),
            filter_bots: config.sidebars.communities.filter_bots.unwrap_or_default(),
            has_filter_groups: config.sidebars.communities.filter_groups.is_some(),
            filter_groups: config.sidebars.communities.filter_groups.unwrap_or_default(),
            has_filter_server_notices: config.sidebars.communities.filter_server_notices.is_some(),
            filter_server_notices: config
                .sidebars
                .communities
                .filter_server_notices
                .unwrap_or_default(),
            has_filter_low_priority: config.sidebars.communities.filter_low_priority.is_some(),
            filter_low_priority: config
                .sidebars
                .communities
                .filter_low_priority
                .unwrap_or_default(),
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
            positioning: config.timeline.messages.positioning.to_storage_string(),
            user_color_coding_policy: config.timeline.user_color_coding_policy.to_storage_string(),
            has_layout_small_avatars: config.timeline.messages.layout.small_avatars.is_some(),
            layout_small_avatars: config
                .timeline
                .messages
                .layout
                .small_avatars
                .unwrap_or_default(),
            has_layout_show_own_avatar: config.timeline.messages.layout.show_own_avatar.is_some(),
            layout_show_own_avatar: config
                .timeline
                .messages
                .layout
                .show_own_avatar
                .unwrap_or_default(),
            sender_username: config.timeline.messages.sender_username.to_storage_string(),
            has_emoji_only_enlarge: config.timeline.messages.emoji_only_enlarge.is_some(),
            emoji_only_enlarge: config
                .timeline
                .messages
                .emoji_only_enlarge
                .unwrap_or_default(),
            has_hover_highlight: config.timeline.messages.hover_highlight.is_some(),
            hover_highlight: config.timeline.messages.hover_highlight.unwrap_or_default(),
        },
        formatted: ffi::SettingsConfigTimelineFormattedSection {
            has_code_syntax_highlighting: config
                .timeline
                .formatted
                .code_syntax_highlighting
                .is_some(),
            code_syntax_highlighting: config
                .timeline
                .formatted
                .code_syntax_highlighting
                .unwrap_or_default(),
        },
        typing: ffi::SettingsConfigTimelineTypingSection {
            has_show_enabled: config.timeline.typing.show_enabled.is_some(),
            show_enabled: config.timeline.typing.show_enabled.unwrap_or_default(),
        },
        read_receipts: ffi::SettingsConfigTimelineReadReceiptsSection {
            has_enabled: config.timeline.read_receipts.enabled.is_some(),
            enabled: config.timeline.read_receipts.enabled.unwrap_or_default(),
        },
        message_actions: ffi::SettingsConfigTimelineMessageActionsSection {
            activation_policy: config
                .timeline
                .message_actions
                .activation_policy
                .to_storage_string(),
            pinned_reactions: config.timeline.message_actions.pinned_reactions.clone(),
        },
        media: ffi::SettingsConfigTimelineMediaSection {
            has_effects_enabled: config.timeline.media.effects_enabled.is_some(),
            effects_enabled: config.timeline.media.effects_enabled.unwrap_or_default(),
            has_animate_on_hover: config.timeline.media.animate_on_hover.is_some(),
            animate_on_hover: config.timeline.media.animate_on_hover.unwrap_or_default(),
            image_display: config.timeline.media.image_display.to_storage_string(),
            has_open_images_external: config.timeline.media.open_images_external.is_some(),
            open_images_external: config.timeline.media.open_images_external.unwrap_or_default(),
            has_open_videos_external: config.timeline.media.open_videos_external.is_some(),
            open_videos_external: config.timeline.media.open_videos_external.unwrap_or_default(),
            has_autoplay_gif_videos: config.timeline.media.autoplay_gif_videos.is_some(),
            autoplay_gif_videos: config.timeline.media.autoplay_gif_videos.unwrap_or_default(),
            has_open_audio_external: config.timeline.media.open_audio_external.is_some(),
            open_audio_external: config.timeline.media.open_audio_external.unwrap_or_default(),
            has_default_audio_playback_speed: config
                .timeline
                .media
                .default_audio_playback_speed
                .is_some(),
            default_audio_playback_speed: config
                .timeline
                .media
                .default_audio_playback_speed
                .unwrap_or_default(),
        },
        hidden_events: ffi::SettingsConfigTimelineHiddenEventsSection {
            has_global: config.timeline.hidden_events.global.is_some(),
            global: config.timeline.hidden_events.global.clone().unwrap_or_default(),
            by_room,
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

pub(crate) fn ffi_config_privacy_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigPrivacySection {
    ffi::SettingsConfigPrivacySection {
        window_focus_blur: ffi::SettingsConfigPrivacyWindowFocusBlurSection {
            has_enabled: config.privacy.window_focus_blur.enabled.is_some(),
            enabled: config.privacy.window_focus_blur.enabled.unwrap_or_default(),
            has_delay_seconds: config.privacy.window_focus_blur.delay_seconds.is_some(),
            delay_seconds: config.privacy.window_focus_blur.delay_seconds.unwrap_or_default(),
        },
        maintenance: ffi::SettingsConfigPrivacyMaintenanceSection {
            has_expire_events: config.privacy.maintenance.expire_events.is_some(),
            expire_events: config.privacy.maintenance.expire_events.unwrap_or_default(),
        },
    }
}

pub(crate) fn ffi_config_encryption_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigEncryptionSection {
    ffi::SettingsConfigEncryptionSection {
        key_sharing: ffi::SettingsConfigEncryptionKeySharingSection {
            has_only_verified_users: config.encryption.key_sharing.only_verified_users.is_some(),
            only_verified_users: config
                .encryption
                .key_sharing
                .only_verified_users
                .unwrap_or_default(),
            has_share_with_trusted: config.encryption.key_sharing.share_with_trusted.is_some(),
            share_with_trusted: config
                .encryption
                .key_sharing
                .share_with_trusted
                .unwrap_or_default(),
        },
        backup: ffi::SettingsConfigEncryptionBackupSection {
            online: ffi::SettingsConfigEncryptionBackupOnlineSection {
                has_enabled: config.encryption.backup.online.enabled.is_some(),
                enabled: config.encryption.backup.online.enabled.unwrap_or_default(),
            },
        },
    }
}

pub(crate) fn ffi_config_calls_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigCallsSection {
    ffi::SettingsConfigCallsSection {
        legacy: ffi::SettingsConfigCallsLegacySection {
            has_enabled: config.calls.legacy.enabled.is_some(),
            enabled: config.calls.legacy.enabled.unwrap_or_default(),
        },
        relay: ffi::SettingsConfigCallsRelaySection {
            has_use_fallback_server: config.calls.relay.use_fallback_server.is_some(),
            use_fallback_server: config.calls.relay.use_fallback_server.unwrap_or_default(),
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
            has_frame_rate: config.calls.screenshare.frame_rate.is_some(),
            frame_rate: config.calls.screenshare.frame_rate.unwrap_or_default(),
            has_picture_in_picture: config.calls.screenshare.picture_in_picture.is_some(),
            picture_in_picture: config.calls.screenshare.picture_in_picture.unwrap_or_default(),
            has_include_remote_video: config.calls.screenshare.include_remote_video.is_some(),
            include_remote_video: config.calls.screenshare.include_remote_video.unwrap_or_default(),
            has_show_cursor: config.calls.screenshare.show_cursor.is_some(),
            show_cursor: config.calls.screenshare.show_cursor.unwrap_or_default(),
        },
    }
}

pub(crate) fn ffi_config_notifications_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigNotificationsSection {
    ffi::SettingsConfigNotificationsSection {
        has_enabled: config.notifications.enabled.is_some(),
        enabled: config.notifications.enabled.unwrap_or_default(),
        has_attention_on_incoming: config.notifications.attention_on_incoming.is_some(),
        attention_on_incoming: config.notifications.attention_on_incoming.unwrap_or_default(),
        message_content_policy: config.notifications.message_content_policy.to_storage_string(),
    }
}

pub(crate) fn ffi_config_network_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigNetworkSection {
    ffi::SettingsConfigNetworkSection {
        presence_status_policy: config.network.presence_status_policy.to_storage_string(),
        has_tls_enable_certificate_validation: config
            .network
            .tls_enable_certificate_validation
            .is_some(),
        tls_enable_certificate_validation: config
            .network
            .tls_enable_certificate_validation
            .unwrap_or_default(),
        has_mrs_enabled: config.network.mrs_enabled.is_some(),
        mrs_enabled: config.network.mrs_enabled.unwrap_or_default(),
        mrs_server_name: config.network.mrs_server_name.clone(),
        has_http3_enabled: config.network.http3_enabled.is_some(),
        http3_enabled: config.network.http3_enabled.unwrap_or_default(),
    }
}

pub(crate) fn ffi_config_integrations_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigIntegrationsSection {
    ffi::SettingsConfigIntegrationsSection {
        has_system_tray_enabled: config.integrations.system_tray_enabled.is_some(),
        system_tray_enabled: config.integrations.system_tray_enabled.unwrap_or_default(),
        has_system_tray_autostart: config.integrations.system_tray_autostart.is_some(),
        system_tray_autostart: config.integrations.system_tray_autostart.unwrap_or_default(),
        dbus_api_access: config.integrations.dbus_api_access.to_storage_string(),
        browser_command: config.integrations.browser_command.clone(),
    }
}

pub(crate) fn ffi_config_composer_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigComposerSection {
    ffi::SettingsConfigComposerSection {
        has_input_markdown_to_html_enabled: config.composer.input_markdown_to_html_enabled.is_some(),
        input_markdown_to_html_enabled: config
            .composer
            .input_markdown_to_html_enabled
            .unwrap_or_default(),
        input_send_key: config.composer.input_send_key.to_storage_string(),
        input_auto_replace_emoji: config.composer.input_auto_replace_emoji.to_storage_string(),
        input_emoji_preferred_gender: config
            .composer
            .input_emoji_preferred_gender
            .to_storage_string(),
        input_emoji_preferred_skin_tone: config
            .composer
            .input_emoji_preferred_skin_tone
            .to_storage_string(),
        has_input_inline_emoji_picker_enabled: config
            .composer
            .input_inline_emoji_picker_enabled
            .is_some(),
        input_inline_emoji_picker_enabled: config
            .composer
            .input_inline_emoji_picker_enabled
            .unwrap_or_default(),
        has_input_inline_room_picker_enabled: config
            .composer
            .input_inline_room_picker_enabled
            .is_some(),
        input_inline_room_picker_enabled: config
            .composer
            .input_inline_room_picker_enabled
            .unwrap_or_default(),
        has_input_inline_user_picker_enabled: config
            .composer
            .input_inline_user_picker_enabled
            .is_some(),
        input_inline_user_picker_enabled: config
            .composer
            .input_inline_user_picker_enabled
            .unwrap_or_default(),
        has_typing_send_enabled: config.composer.typing_send_enabled.is_some(),
        typing_send_enabled: config.composer.typing_send_enabled.unwrap_or_default(),
        has_extras_stickers_enabled: config.composer.extras_stickers_enabled.is_some(),
        extras_stickers_enabled: config.composer.extras_stickers_enabled.unwrap_or_default(),
    }
}

pub(crate) fn ffi_loaded_config(snapshot: settings::config::LoadedConfig) -> ffi::SettingsLoadedConfig {
    ffi::SettingsLoadedConfig {
        ui: ffi_config_ui_section(&snapshot.config),
        sidebars: ffi_config_sidebars_section(&snapshot.config),
        timeline: ffi_config_timeline_section(&snapshot.config),
        secrets: ffi_config_secrets_section(&snapshot.config),
        privacy: ffi_config_privacy_section(&snapshot.config),
        encryption: ffi_config_encryption_section(&snapshot.config),
        calls: ffi_config_calls_section(&snapshot.config),
        notifications: ffi_config_notifications_section(&snapshot.config),
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
        sidebars_room_list_width_px: snapshot.sidebars_room_list_width_px,
        sidebars_communities_width_px: snapshot.sidebars_communities_width_px,
        current_filter_id: snapshot.current_filter_id,
        current_room_id: snapshot.current_room_id,
        global_excludes: snapshot.global_excludes,
        badges_hidden_filters: snapshot.badges_hidden_filters,
        hidden_pins: snapshot.hidden_pins,
        hidden_widgets: snapshot.hidden_widgets,
        collapsed_spaces: snapshot.collapsed_spaces,
        composer_drafts_by_room: snapshot.composer_drafts_by_room,
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
        has_scale_factor: section.has_scale_factor,
        scale_factor: section.scale_factor,
        theme_slug: section.theme_slug.clone(),
        has_font_size_pt: section.has_font_size_pt,
        font_size_pt: section.font_size_pt,
        font_family: section.font_family.clone(),
        font_emoji_family: section.font_emoji_family.clone(),
        has_motion_animations_enabled: section.has_motion_animations_enabled,
        motion_animations_enabled: section.motion_animations_enabled,
        input_mode: section.input_mode.clone(),
        has_input_touch_swipe_gestures_enabled: section.has_input_touch_swipe_gestures_enabled,
        input_touch_swipe_gestures_enabled: section.input_touch_swipe_gestures_enabled,
        has_layout_content_max_width_px: section.has_layout_content_max_width_px,
        layout_content_max_width_px: section.layout_content_max_width_px,
        has_layout_compact_mode: section.has_layout_compact_mode,
        layout_compact_mode: section.layout_compact_mode,
        has_avatars_circular: section.has_avatars_circular,
        avatars_circular: section.avatars_circular,
        scrollbar_policy: section.scrollbar_policy.clone(),
        default_avatar_style: section.default_avatar_style.clone(),
    }
}

fn clone_config_sidebars_section(
    section: &ffi::SettingsConfigSidebarsSection,
) -> ffi::SettingsConfigSidebarsSection {
    ffi::SettingsConfigSidebarsSection {
        room_list: ffi::SettingsConfigSidebarsRoomListSection {
            has_show_last_message_time: section.room_list.has_show_last_message_time,
            show_last_message_time: section.room_list.show_last_message_time,
            last_message_preview: section.room_list.last_message_preview.clone(),
            has_show_community_counts: section.room_list.has_show_community_counts,
            show_community_counts: section.room_list.show_community_counts,
            sort: section.room_list.sort.clone(),
            unread_detection_policy: section.room_list.unread_detection_policy.clone(),
        },
        communities: ffi::SettingsConfigSidebarsCommunitiesSection {
            has_visible: section.communities.has_visible,
            visible: section.communities.visible,
            has_filter_favourites: section.communities.has_filter_favourites,
            filter_favourites: section.communities.filter_favourites,
            has_filter_people: section.communities.has_filter_people,
            filter_people: section.communities.filter_people,
            has_filter_bots: section.communities.has_filter_bots,
            filter_bots: section.communities.filter_bots,
            has_filter_groups: section.communities.has_filter_groups,
            filter_groups: section.communities.filter_groups,
            has_filter_server_notices: section.communities.has_filter_server_notices,
            filter_server_notices: section.communities.filter_server_notices,
            has_filter_low_priority: section.communities.has_filter_low_priority,
            filter_low_priority: section.communities.filter_low_priority,
        },
    }
}

fn clone_config_timeline_section(
    section: &ffi::SettingsConfigTimelineSection,
) -> ffi::SettingsConfigTimelineSection {
    ffi::SettingsConfigTimelineSection {
        messages: ffi::SettingsConfigTimelineMessagesSection {
            style: section.messages.style.clone(),
            positioning: section.messages.positioning.clone(),
            user_color_coding_policy: section.messages.user_color_coding_policy.clone(),
            has_layout_small_avatars: section.messages.has_layout_small_avatars,
            layout_small_avatars: section.messages.layout_small_avatars,
            has_layout_show_own_avatar: section.messages.has_layout_show_own_avatar,
            layout_show_own_avatar: section.messages.layout_show_own_avatar,
            sender_username: section.messages.sender_username.clone(),
            has_emoji_only_enlarge: section.messages.has_emoji_only_enlarge,
            emoji_only_enlarge: section.messages.emoji_only_enlarge,
            has_hover_highlight: section.messages.has_hover_highlight,
            hover_highlight: section.messages.hover_highlight,
        },
        formatted: ffi::SettingsConfigTimelineFormattedSection {
            has_code_syntax_highlighting: section.formatted.has_code_syntax_highlighting,
            code_syntax_highlighting: section.formatted.code_syntax_highlighting,
        },
        typing: ffi::SettingsConfigTimelineTypingSection {
            has_show_enabled: section.typing.has_show_enabled,
            show_enabled: section.typing.show_enabled,
        },
        read_receipts: ffi::SettingsConfigTimelineReadReceiptsSection {
            has_enabled: section.read_receipts.has_enabled,
            enabled: section.read_receipts.enabled,
        },
        message_actions: ffi::SettingsConfigTimelineMessageActionsSection {
            activation_policy: section.message_actions.activation_policy.clone(),
            pinned_reactions: section.message_actions.pinned_reactions.clone(),
        },
        media: ffi::SettingsConfigTimelineMediaSection {
            has_effects_enabled: section.media.has_effects_enabled,
            effects_enabled: section.media.effects_enabled,
            has_animate_on_hover: section.media.has_animate_on_hover,
            animate_on_hover: section.media.animate_on_hover,
            image_display: section.media.image_display.clone(),
            has_open_images_external: section.media.has_open_images_external,
            open_images_external: section.media.open_images_external,
            has_open_videos_external: section.media.has_open_videos_external,
            open_videos_external: section.media.open_videos_external,
            has_autoplay_gif_videos: section.media.has_autoplay_gif_videos,
            autoplay_gif_videos: section.media.autoplay_gif_videos,
            has_open_audio_external: section.media.has_open_audio_external,
            open_audio_external: section.media.open_audio_external,
            has_default_audio_playback_speed: section.media.has_default_audio_playback_speed,
            default_audio_playback_speed: section.media.default_audio_playback_speed,
        },
        hidden_events: ffi::SettingsConfigTimelineHiddenEventsSection {
            has_global: section.hidden_events.has_global,
            global: section
                .hidden_events
                .global
                .iter()
                .map(|value| value.clone())
                .collect(),
            by_room: clone_string_list_map_entries(&section.hidden_events.by_room),
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

fn clone_config_privacy_section(
    section: &ffi::SettingsConfigPrivacySection,
) -> ffi::SettingsConfigPrivacySection {
    ffi::SettingsConfigPrivacySection {
        window_focus_blur: ffi::SettingsConfigPrivacyWindowFocusBlurSection {
            has_enabled: section.window_focus_blur.has_enabled,
            enabled: section.window_focus_blur.enabled,
            has_delay_seconds: section.window_focus_blur.has_delay_seconds,
            delay_seconds: section.window_focus_blur.delay_seconds,
        },
        maintenance: ffi::SettingsConfigPrivacyMaintenanceSection {
            has_expire_events: section.maintenance.has_expire_events,
            expire_events: section.maintenance.expire_events,
        },
    }
}

fn clone_config_encryption_section(
    section: &ffi::SettingsConfigEncryptionSection,
) -> ffi::SettingsConfigEncryptionSection {
    ffi::SettingsConfigEncryptionSection {
        key_sharing: ffi::SettingsConfigEncryptionKeySharingSection {
            has_only_verified_users: section.key_sharing.has_only_verified_users,
            only_verified_users: section.key_sharing.only_verified_users,
            has_share_with_trusted: section.key_sharing.has_share_with_trusted,
            share_with_trusted: section.key_sharing.share_with_trusted,
        },
        backup: ffi::SettingsConfigEncryptionBackupSection {
            online: ffi::SettingsConfigEncryptionBackupOnlineSection {
                has_enabled: section.backup.online.has_enabled,
                enabled: section.backup.online.enabled,
            },
        },
    }
}

fn clone_config_calls_section(
    section: &ffi::SettingsConfigCallsSection,
) -> ffi::SettingsConfigCallsSection {
    ffi::SettingsConfigCallsSection {
        legacy: ffi::SettingsConfigCallsLegacySection {
            has_enabled: section.legacy.has_enabled,
            enabled: section.legacy.enabled,
        },
        relay: ffi::SettingsConfigCallsRelaySection {
            has_use_fallback_server: section.relay.has_use_fallback_server,
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
            has_frame_rate: section.screenshare.has_frame_rate,
            frame_rate: section.screenshare.frame_rate,
            has_picture_in_picture: section.screenshare.has_picture_in_picture,
            picture_in_picture: section.screenshare.picture_in_picture,
            has_include_remote_video: section.screenshare.has_include_remote_video,
            include_remote_video: section.screenshare.include_remote_video,
            has_show_cursor: section.screenshare.has_show_cursor,
            show_cursor: section.screenshare.show_cursor,
        },
    }
}

fn clone_config_notifications_section(
    section: &ffi::SettingsConfigNotificationsSection,
) -> ffi::SettingsConfigNotificationsSection {
    ffi::SettingsConfigNotificationsSection {
        has_enabled: section.has_enabled,
        enabled: section.enabled,
        has_attention_on_incoming: section.has_attention_on_incoming,
        attention_on_incoming: section.attention_on_incoming,
        message_content_policy: section.message_content_policy.clone(),
    }
}

fn clone_config_network_section(
    section: &ffi::SettingsConfigNetworkSection,
) -> ffi::SettingsConfigNetworkSection {
    ffi::SettingsConfigNetworkSection {
        presence_status_policy: section.presence_status_policy.clone(),
        has_tls_enable_certificate_validation: section.has_tls_enable_certificate_validation,
        tls_enable_certificate_validation: section.tls_enable_certificate_validation,
        has_mrs_enabled: section.has_mrs_enabled,
        mrs_enabled: section.mrs_enabled,
        mrs_server_name: section.mrs_server_name.clone(),
        has_http3_enabled: section.has_http3_enabled,
        http3_enabled: section.http3_enabled,
    }
}

fn clone_config_integrations_section(
    section: &ffi::SettingsConfigIntegrationsSection,
) -> ffi::SettingsConfigIntegrationsSection {
    ffi::SettingsConfigIntegrationsSection {
        has_system_tray_enabled: section.has_system_tray_enabled,
        system_tray_enabled: section.system_tray_enabled,
        has_system_tray_autostart: section.has_system_tray_autostart,
        system_tray_autostart: section.system_tray_autostart,
        dbus_api_access: section.dbus_api_access.clone(),
        browser_command: section.browser_command.clone(),
    }
}

fn clone_config_composer_section(
    section: &ffi::SettingsConfigComposerSection,
) -> ffi::SettingsConfigComposerSection {
    ffi::SettingsConfigComposerSection {
        has_input_markdown_to_html_enabled: section.has_input_markdown_to_html_enabled,
        input_markdown_to_html_enabled: section.input_markdown_to_html_enabled,
        input_send_key: section.input_send_key.clone(),
        input_auto_replace_emoji: section.input_auto_replace_emoji.clone(),
        input_emoji_preferred_gender: section.input_emoji_preferred_gender.clone(),
        input_emoji_preferred_skin_tone: section.input_emoji_preferred_skin_tone.clone(),
        has_input_inline_emoji_picker_enabled: section.has_input_inline_emoji_picker_enabled,
        input_inline_emoji_picker_enabled: section.input_inline_emoji_picker_enabled,
        has_input_inline_room_picker_enabled: section.has_input_inline_room_picker_enabled,
        input_inline_room_picker_enabled: section.input_inline_room_picker_enabled,
        has_input_inline_user_picker_enabled: section.has_input_inline_user_picker_enabled,
        input_inline_user_picker_enabled: section.input_inline_user_picker_enabled,
        has_typing_send_enabled: section.has_typing_send_enabled,
        typing_send_enabled: section.typing_send_enabled,
        has_extras_stickers_enabled: section.has_extras_stickers_enabled,
        extras_stickers_enabled: section.extras_stickers_enabled,
    }
}

pub(in crate::settings) fn loaded_config_to_snapshot(
    loaded: &ffi::SettingsLoadedConfig,
) -> ffi::SettingsConfigSnapshot {
    ffi::SettingsConfigSnapshot {
        ui: clone_config_ui_section(&loaded.ui),
        sidebars: clone_config_sidebars_section(&loaded.sidebars),
        timeline: clone_config_timeline_section(&loaded.timeline),
        secrets: clone_config_secrets_section(&loaded.secrets),
        privacy: clone_config_privacy_section(&loaded.privacy),
        encryption: clone_config_encryption_section(&loaded.encryption),
        calls: clone_config_calls_section(&loaded.calls),
        notifications: clone_config_notifications_section(&loaded.notifications),
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
        sidebars: clone_config_sidebars_section(&loaded.sidebars),
        timeline: clone_config_timeline_section(&loaded.timeline),
        secrets: clone_config_secrets_section(&loaded.secrets),
        privacy: clone_config_privacy_section(&loaded.privacy),
        encryption: clone_config_encryption_section(&loaded.encryption),
        calls: clone_config_calls_section(&loaded.calls),
        notifications: clone_config_notifications_section(&loaded.notifications),
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
        sidebars: clone_config_sidebars_section(&snapshot.sidebars),
        timeline: clone_config_timeline_section(&snapshot.timeline),
        secrets: clone_config_secrets_section(&snapshot.secrets),
        privacy: clone_config_privacy_section(&snapshot.privacy),
        encryption: clone_config_encryption_section(&snapshot.encryption),
        calls: clone_config_calls_section(&snapshot.calls),
        notifications: clone_config_notifications_section(&snapshot.notifications),
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
        sidebars_room_list_width_px: loaded.sidebars_room_list_width_px,
        sidebars_communities_width_px: loaded.sidebars_communities_width_px,
        current_filter_id: loaded.current_filter_id.clone(),
        current_room_id: loaded.current_room_id.clone(),
        global_excludes: loaded.global_excludes.iter().cloned().collect(),
        badges_hidden_filters: loaded.badges_hidden_filters.iter().cloned().collect(),
        hidden_pins: loaded.hidden_pins.iter().cloned().collect(),
        hidden_widgets: loaded.hidden_widgets.iter().cloned().collect(),
        collapsed_spaces: loaded.collapsed_spaces.iter().cloned().collect(),
        composer_drafts_by_room: loaded
            .composer_drafts_by_room
            .iter()
            .map(|entry| ffi::SettingsStringMapEntry {
                key: entry.key.clone(),
                value: entry.value.clone(),
            })
            .collect(),
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
        sidebars_room_list_width_px: snapshot.sidebars_room_list_width_px,
        sidebars_communities_width_px: snapshot.sidebars_communities_width_px,
        current_filter_id: snapshot.current_filter_id.clone(),
        current_room_id: snapshot.current_room_id.clone(),
        global_excludes: snapshot.global_excludes.iter().cloned().collect(),
        badges_hidden_filters: snapshot.badges_hidden_filters.iter().cloned().collect(),
        hidden_pins: snapshot.hidden_pins.iter().cloned().collect(),
        hidden_widgets: snapshot.hidden_widgets.iter().cloned().collect(),
        collapsed_spaces: snapshot.collapsed_spaces.iter().cloned().collect(),
        composer_drafts_by_room: snapshot
            .composer_drafts_by_room
            .iter()
            .map(|entry| ffi::SettingsStringMapEntry {
                key: entry.key.clone(),
                value: entry.value.clone(),
            })
            .collect(),
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
