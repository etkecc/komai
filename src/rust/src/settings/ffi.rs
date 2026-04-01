// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use crate::{ffi, settings};

pub(crate) fn settings_load_startup_snapshot_from_path(config_path: &str) -> ffi::SettingsStartupSnapshot {
    let snapshot = settings::startup::snapshot_from_config_path(config_path);

    ffi::SettingsStartupSnapshot {
        has_ui_scale_factor: snapshot.ui_scale_factor.is_some(),
        ui_scale_factor: snapshot.ui_scale_factor.unwrap_or_default(),
    }
}

pub(crate) fn settings_load_config_overview(config_text: &str) -> ffi::SettingsConfigOverview {
    let config = settings::config::parse_config_text(config_text);

    ffi::SettingsConfigOverview {
        has_ui_scale_factor: config.ui.scale.factor.is_some(),
        ui_scale_factor: config.ui.scale.factor.unwrap_or_default(),
        theme_slug: config.ui.theme.slug,
        secrets_provider: config.secrets.provider,
    }
}

pub(crate) fn settings_load_config_overview_from_path(config_path: &str) -> ffi::SettingsConfigOverview {
    settings_load_config_overview(&settings::storage::read_text_file(config_path, "config"))
}

pub(crate) fn settings_encode_string_map_yaml(entries: &Vec<ffi::SettingsStringMapEntry>) -> String {
    settings::secrets::encode_string_map_yaml(entries.as_slice())
}

pub(crate) fn settings_decode_string_map_yaml(serialized: &str) -> Vec<ffi::SettingsStringMapEntry> {
    settings::secrets::decode_string_map_yaml(serialized)
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

pub(crate) fn settings_load_named_string_map_from_path(
    path: &str,
    label: &str,
    root_key: &str,
) -> Vec<ffi::SettingsStringMapEntry> {
    settings::secrets::load_named_string_map_from_path(path, label, root_key)
}

pub(crate) fn settings_write_named_string_map_to_path(
    path: &str,
    root_key: &str,
    entries: &Vec<ffi::SettingsStringMapEntry>,
    owner_read_write_only: bool,
) -> bool {
    settings::secrets::write_named_string_map_to_path(
        path,
        root_key,
        entries.as_slice(),
        owner_read_write_only,
    )
}

pub(crate) fn settings_encode_config_yaml(snapshot: &ffi::SettingsConfigSnapshot) -> String {
    settings::config::encode_config_yaml(snapshot)
}

pub(crate) fn settings_write_config_snapshot_to_path(
    config_path: &str,
    snapshot: &ffi::SettingsConfigSnapshot,
) -> bool {
    settings::config::write_config_snapshot_to_path(config_path, snapshot)
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
        input_mode: config.ui.input.mode.clone(),
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
        scrollbar_policy: config.ui.scrollbar_policy.clone(),
        default_avatar_style: config.ui.avatars.default_avatar_style.clone(),
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
        provider: config.secrets.provider.clone(),
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
        message_content_policy: config.notifications.message_content_policy.clone(),
    }
}

pub(crate) fn ffi_config_network_section(
    config: &settings::config::Config,
) -> ffi::SettingsConfigNetworkSection {
    ffi::SettingsConfigNetworkSection {
        presence_status_policy: config.network.presence_status_policy.clone(),
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
        dbus_api_access: config.integrations.dbus_api_access.clone(),
        browser_command: config.integrations.browser_command.clone(),
    }
}

pub(crate) fn ffi_loaded_config(snapshot: settings::config::LoadedConfig) -> ffi::SettingsLoadedConfig {
    ffi::SettingsLoadedConfig {
        ui: ffi_config_ui_section(&snapshot.config),
        timeline: ffi_config_timeline_section(&snapshot.config),
        secrets: ffi_config_secrets_section(&snapshot.config),
        privacy: ffi_config_privacy_section(&snapshot.config),
        calls: ffi_config_calls_section(&snapshot.config),
        notifications: ffi_config_notifications_section(&snapshot.config),
        network: ffi_config_network_section(&snapshot.config),
        integrations: ffi_config_integrations_section(&snapshot.config),
        values: snapshot.values,
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

pub(crate) fn settings_load_profile_snapshot_from_paths(
    config_path: &str,
    session_path: &str,
    state_path: &str,
    include_session: bool,
) -> ffi::SettingsLoadedProfile {
    let snapshot = settings::profile::load_profile_snapshot_from_paths(
        config_path,
        session_path,
        state_path,
        include_session,
    );

    ffi::SettingsLoadedProfile {
        config: ffi_loaded_config(snapshot.config),
        session: ffi_loaded_session(snapshot.session),
        state: ffi_loaded_state(snapshot.state),
    }
}

pub(crate) fn settings_load_session_snapshot(session_text: &str) -> ffi::SettingsLoadedSession {
    ffi_loaded_session(settings::session::load_session_snapshot(session_text))
}

pub(crate) fn settings_load_session_snapshot_from_path(session_path: &str) -> ffi::SettingsLoadedSession {
    ffi_loaded_session(settings::session::load_session_snapshot_from_path(session_path))
}

pub(crate) fn settings_encode_session_yaml(user_id: &str, homeserver: &str, device_id: &str) -> String {
    settings::session::encode_session_yaml(user_id, homeserver, device_id)
}

pub(crate) fn settings_write_session_snapshot_to_path(
    session_path: &str,
    user_id: &str,
    homeserver: &str,
    device_id: &str,
) -> bool {
    settings::session::write_session_snapshot_to_path(session_path, user_id, homeserver, device_id)
}

pub(crate) fn settings_load_state_snapshot(state_text: &str) -> ffi::SettingsLoadedState {
    ffi_loaded_state(settings::state::load_state_snapshot(state_text))
}

pub(crate) fn settings_encode_state_yaml(snapshot: &ffi::SettingsStateSnapshot) -> String {
    settings::state::encode_state_yaml(snapshot)
}

pub(crate) fn settings_write_state_snapshot_to_path(
    state_path: &str,
    snapshot: &ffi::SettingsStateSnapshot,
) -> bool {
    settings::state::write_state_snapshot_to_path(state_path, snapshot)
}

pub(crate) fn settings_write_loaded_config_to_path(
    config_path: &str,
    loaded: &ffi::SettingsLoadedConfig,
) -> bool {
    settings::storage::write_text_file(config_path, &loaded.serialized_yaml, false)
}

pub(crate) fn settings_write_loaded_session_to_path(
    session_path: &str,
    loaded: &ffi::SettingsLoadedSession,
) -> bool {
    settings::storage::write_text_file(session_path, &loaded.serialized_yaml, false)
}

pub(crate) fn settings_write_loaded_state_to_path(
    state_path: &str,
    loaded: &ffi::SettingsLoadedState,
) -> bool {
    settings::storage::write_text_file(state_path, &loaded.serialized_yaml, false)
}
