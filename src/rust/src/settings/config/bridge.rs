// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use serde_yaml_ng::{Number, Value};

use crate::ffi::SettingsConfigSnapshot;
use crate::settings::yaml;

use super::model::{CONFIG_SCHEMA_VERSION_PATH, CURRENT_CONFIG_SCHEMA_VERSION, LoadedConfig};
use super::tree;

pub(super) fn encode_config_yaml(snapshot: &SettingsConfigSnapshot) -> String {
    let mut root = yaml::empty_mapping();
    yaml::set_value(
        &mut root,
        &CONFIG_SCHEMA_VERSION_PATH,
        yaml::number_value(CURRENT_CONFIG_SCHEMA_VERSION),
    );

    yaml::set_value(
        &mut root,
        &["ui", "scale", "factor"],
        serde_yaml_ng::to_value(snapshot.ui.scale_factor).unwrap_or(Value::Null),
    );
    yaml::set_value(
        &mut root,
        &["ui", "theme", "slug"],
        Value::String(snapshot.ui.theme_slug.clone()),
    );
    // skip when empty: a blank mode round-trips to "absent", so don't persist the noise.
    if !snapshot.ui.theme_mode.is_empty() {
        yaml::set_value(
            &mut root,
            &["ui", "theme", "mode"],
            Value::String(snapshot.ui.theme_mode.clone()),
        );
    }
    yaml::set_value(
        &mut root,
        &["ui", "font", "size_pt"],
        serde_yaml_ng::to_value(snapshot.ui.font_size_pt).unwrap_or(Value::Null),
    );
    yaml::set_value(
        &mut root,
        &["ui", "font", "family"],
        Value::String(snapshot.ui.font_family.clone()),
    );
    yaml::set_value(
        &mut root,
        &["ui", "font", "emoji_family"],
        Value::String(snapshot.ui.font_emoji_family.clone()),
    );
    yaml::set_value(
        &mut root,
        &["ui", "motion", "enable_animations"],
        Value::Bool(snapshot.ui.motion_animations_enabled),
    );
    yaml::set_value(
        &mut root,
        &["ui", "layout", "density"],
        Value::String(snapshot.ui.layout_density.clone()),
    );
    yaml::set_value(
        &mut root,
        &["ui", "avatars", "circular"],
        Value::Bool(snapshot.ui.avatars_circular),
    );
    yaml::set_value(
        &mut root,
        &["ui", "avatars", "default_avatar_style"],
        Value::String(snapshot.ui.default_avatar_style.clone()),
    );
    yaml::set_value(
        &mut root,
        &["ui", "scrollbar_policy"],
        Value::String(snapshot.ui.scrollbar_policy.clone()),
    );
    yaml::set_value(
        &mut root,
        &["ui", "language"],
        Value::String(snapshot.ui.language.clone()),
    );

    yaml::set_value(
        &mut root,
        &["navigation", "room_list", "show_last_message_timestamp"],
        Value::Bool(snapshot.navigation.room_list.show_last_message_time),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "room_list", "last_message_preview"],
        Value::String(snapshot.navigation.room_list.last_message_preview.clone()),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "room_list", "show_unread_indicators"],
        Value::Bool(snapshot.navigation.room_list.show_unread_indicators),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "room_list", "sort"],
        Value::String(snapshot.navigation.room_list.sort.clone()),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "room_list", "opening_policy"],
        Value::String(snapshot.navigation.room_list.opening_policy.clone()),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "communities", "show_unread_indicators"],
        Value::Bool(snapshot.navigation.communities.show_unread_indicators),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "communities", "filters", "favourites"],
        Value::Bool(snapshot.navigation.communities.filter_favourites),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "communities", "filters", "people"],
        Value::Bool(snapshot.navigation.communities.filter_people),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "communities", "filters", "bots"],
        Value::Bool(snapshot.navigation.communities.filter_bots),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "communities", "filters", "groups"],
        Value::Bool(snapshot.navigation.communities.filter_groups),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "communities", "filters", "server_notices"],
        Value::Bool(snapshot.navigation.communities.filter_server_notices),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "communities", "filters", "low_priority"],
        Value::Bool(snapshot.navigation.communities.filter_low_priority),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "tabs", "auto_hide_with_single_tab"],
        Value::Bool(snapshot.navigation.tabs.auto_hide_with_single_tab),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "tabs", "show_pin_button"],
        Value::String(snapshot.navigation.tabs.show_pin_button.clone()),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "tabs", "pinned_tab_label"],
        Value::String(snapshot.navigation.tabs.pinned_tab_label.clone()),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "tabs", "tab_label"],
        Value::String(snapshot.navigation.tabs.tab_label.clone()),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "tabs", "preferred_width_px"],
        Value::Number(Number::from(snapshot.navigation.tabs.preferred_width_px)),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "tabs", "minimum_width_px"],
        Value::Number(Number::from(snapshot.navigation.tabs.minimum_width_px)),
    );
    yaml::set_value(
        &mut root,
        &["navigation", "tabs", "max_recently_closed_timelines"],
        Value::Number(Number::from(snapshot.navigation.tabs.max_recently_closed_timelines)),
    );

    yaml::set_value(
        &mut root,
        &["timeline", "messages", "style"],
        Value::String(snapshot.timeline.messages.style.clone()),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "layout", "positioning"],
        Value::String(snapshot.timeline.messages.layout_positioning.clone()),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "user_color_coding_policy"],
        Value::String(snapshot.timeline.messages.user_color_coding_policy.clone()),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "layout", "avatar_size"],
        Value::String(snapshot.timeline.messages.layout_avatar_size.clone()),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "layout", "show_own_avatar"],
        Value::Bool(snapshot.timeline.messages.layout_show_own_avatar),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "layout", "max_width_percent"],
        Value::Number(Number::from(snapshot.timeline.messages.layout_max_width_percent)),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "layout", "adaptive_positioning_breakpoint_px"],
        Value::Number(Number::from(
            snapshot
                .timeline
                .messages
                .layout_adaptive_positioning_breakpoint_px,
        )),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "sender_username"],
        Value::String(snapshot.timeline.messages.sender_username.clone()),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "emoji_only_enlarge"],
        Value::Bool(snapshot.timeline.messages.emoji_only_enlarge),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "hover_highlight"],
        Value::Bool(snapshot.timeline.messages.hover_highlight),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "drag_select"],
        Value::Bool(snapshot.timeline.messages.drag_select),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "formatted", "code_syntax_highlighting"],
        Value::Bool(snapshot.timeline.formatted.code_syntax_highlighting),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "typing", "show", "enabled"],
        Value::Bool(snapshot.timeline.typing.show_enabled),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "read_receipts", "global"],
        Value::Bool(snapshot.timeline.read_receipts.global),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "read_receipts", "by_room"],
        tree::bool_map(&snapshot.timeline.read_receipts.by_room),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "actions", "activation_policy"],
        Value::String(snapshot.timeline.message_actions.activation_policy.clone()),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "messages", "actions", "pinned_reactions"],
        Value::String(snapshot.timeline.message_actions.pinned_reactions.clone()),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "media", "effects", "enabled"],
        Value::Bool(snapshot.timeline.media.effects_enabled),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "media", "animate_on_hover"],
        Value::Bool(snapshot.timeline.media.animate_on_hover),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "media", "image_display"],
        Value::String(snapshot.timeline.media.image_display.clone()),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "media", "open_images_external"],
        Value::Bool(snapshot.timeline.media.open_images_external),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "media", "open_videos_external"],
        Value::Bool(snapshot.timeline.media.open_videos_external),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "media", "autoplay_gif_videos"],
        Value::Bool(snapshot.timeline.media.autoplay_gif_videos),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "media", "open_audio_external"],
        Value::Bool(snapshot.timeline.media.open_audio_external),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "media", "default_audio_playback_speed"],
        serde_yaml_ng::to_value(snapshot.timeline.media.default_audio_playback_speed)
            .unwrap_or(Value::Null),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "threads", "collapse_replies", "global"],
        Value::Bool(snapshot.timeline.threads.collapse_replies_global),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "threads", "collapse_replies", "by_room"],
        tree::bool_map(&snapshot.timeline.threads.collapse_replies_by_room),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "date_dividers", "enabled"],
        Value::Bool(snapshot.timeline.date_dividers.enabled),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "room_header", "button_labels"],
        Value::String(snapshot.timeline.room_header.button_labels.clone()),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "hidden_events", "global"],
        Value::Sequence(
            snapshot
                .timeline
                .hidden_events
                .global
                .iter()
                .map(|entry| Value::String(entry.clone()))
                .collect(),
        ),
    );
    yaml::set_value(
        &mut root,
        &["timeline", "hidden_events", "by_room"],
        tree::string_list_map(&snapshot.timeline.hidden_events.by_room),
    );
    yaml::set_value(
        &mut root,
        &["secrets", "provider"],
        Value::String(snapshot.secrets.provider.clone()),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "notifications", "enabled"],
        Value::Bool(snapshot.desktop.notifications.enabled),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "notifications", "attention_on_incoming"],
        Value::Bool(snapshot.desktop.notifications.attention_on_incoming),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "notifications", "message_content_policy"],
        Value::String(snapshot.desktop.notifications.message_content_policy.clone()),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "attention", "window_title", "enabled"],
        Value::Bool(snapshot.desktop.attention.window_title.enabled),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "attention", "app_badge", "enabled"],
        Value::Bool(snapshot.desktop.attention.app_badge.enabled),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "system_tray", "enabled"],
        Value::Bool(snapshot.desktop.system_tray.enabled),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "system_tray", "autostart"],
        Value::Bool(snapshot.desktop.system_tray.autostart),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "system_tray", "icon_style"],
        Value::String(snapshot.desktop.system_tray.icon_style.clone()),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "window_focus_blur", "enabled"],
        Value::Bool(snapshot.desktop.window_focus_blur.enabled),
    );
    yaml::set_value(
        &mut root,
        &["desktop", "window_focus_blur", "delay_seconds"],
        Value::Number(Number::from(snapshot.desktop.window_focus_blur.delay_seconds)),
    );
    yaml::set_value(
        &mut root,
        &["network", "encryption", "only_verified_users"],
        Value::Bool(snapshot.network.encryption.only_verified_users),
    );
    yaml::set_value(
        &mut root,
        &["network", "encryption", "share_with_trusted"],
        Value::Bool(snapshot.network.encryption.share_with_trusted),
    );
    yaml::set_value(
        &mut root,
        &["network", "encryption", "key_backup"],
        Value::Bool(snapshot.network.encryption.key_backup),
    );
    yaml::set_value(
        &mut root,
        &["calls", "legacy", "enabled"],
        Value::Bool(snapshot.calls.legacy.enabled),
    );
    yaml::set_value(
        &mut root,
        &["calls", "element", "enabled"],
        Value::Bool(snapshot.calls.element.enabled),
    );
    yaml::set_value(
        &mut root,
        &["calls", "relay", "use_fallback_server"],
        Value::Bool(snapshot.calls.relay.use_fallback_server),
    );
    yaml::set_value(
        &mut root,
        &["calls", "devices", "microphone"],
        Value::String(snapshot.calls.devices.microphone.clone()),
    );
    yaml::set_value(
        &mut root,
        &["calls", "devices", "camera"],
        Value::String(snapshot.calls.devices.camera.clone()),
    );
    yaml::set_value(
        &mut root,
        &["calls", "devices", "camera_resolution"],
        Value::String(snapshot.calls.devices.camera_resolution.clone()),
    );
    yaml::set_value(
        &mut root,
        &["calls", "devices", "camera_frame_rate"],
        Value::String(snapshot.calls.devices.camera_frame_rate.clone()),
    );
    yaml::set_value(
        &mut root,
        &["calls", "audio", "ringtone"],
        Value::String(snapshot.calls.audio.ringtone.clone()),
    );
    yaml::set_value(
        &mut root,
        &["calls", "screenshare", "frame_rate"],
        Value::Number(Number::from(snapshot.calls.screenshare.frame_rate)),
    );
    yaml::set_value(
        &mut root,
        &["calls", "screenshare", "picture_in_picture"],
        Value::Bool(snapshot.calls.screenshare.picture_in_picture),
    );
    yaml::set_value(
        &mut root,
        &["calls", "screenshare", "include_remote_video"],
        Value::Bool(snapshot.calls.screenshare.include_remote_video),
    );
    yaml::set_value(
        &mut root,
        &["calls", "screenshare", "show_cursor"],
        Value::Bool(snapshot.calls.screenshare.show_cursor),
    );
    yaml::set_value(
        &mut root,
        &["network", "presence", "status_policy"],
        Value::String(snapshot.network.presence_status_policy.clone()),
    );
    yaml::set_value(
        &mut root,
        &["network", "tls", "enable_certificate_validation"],
        Value::Bool(snapshot.network.tls_enable_certificate_validation),
    );
    yaml::set_value(
        &mut root,
        &["network", "mrs", "enabled"],
        Value::Bool(snapshot.network.mrs_enabled),
    );
    yaml::set_value(
        &mut root,
        &["network", "mrs", "server_name"],
        Value::String(snapshot.network.mrs_server_name.clone()),
    );
    yaml::set_value(
        &mut root,
        &["network", "http3", "enabled"],
        Value::Bool(snapshot.network.http3_enabled),
    );
    yaml::set_value(
        &mut root,
        &["integrations", "dbus", "access"],
        Value::String(snapshot.integrations.dbus_api_access.clone()),
    );
    yaml::set_value(
        &mut root,
        &["integrations", "browser", "command"],
        Value::String(snapshot.integrations.browser_command.clone()),
    );
    yaml::set_value(
        &mut root,
        &["integrations", "transcription", "provider"],
        Value::String(snapshot.integrations.transcription_provider.clone()),
    );
    yaml::set_value(
        &mut root,
        &["integrations", "transcription", "api_url"],
        Value::String(snapshot.integrations.transcription_api_url.clone()),
    );
    yaml::set_value(
        &mut root,
        &["integrations", "transcription", "model"],
        Value::String(snapshot.integrations.transcription_model.clone()),
    );
    yaml::set_value(
        &mut root,
        &["integrations", "transcription", "language"],
        Value::String(snapshot.integrations.transcription_language.clone()),
    );
    yaml::set_value(
        &mut root,
        &["integrations", "transcription", "prompt"],
        Value::String(snapshot.integrations.transcription_prompt.clone()),
    );
    yaml::set_value(
        &mut root,
        &["integrations", "transcription", "by_room"],
        tree::transcription_by_room_map(&snapshot.integrations.transcription_by_room),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "markdown_to_html", "enabled"],
        Value::Bool(snapshot.composer.input_markdown_to_html_enabled),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "send_key"],
        Value::String(snapshot.composer.input_send_key.clone()),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "auto_replace_emoji"],
        Value::String(snapshot.composer.input_auto_replace_emoji.clone()),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "emoji", "preferred_gender"],
        Value::String(snapshot.composer.input_emoji_preferred_gender.clone()),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "emoji", "preferred_skin_tone"],
        Value::String(snapshot.composer.input_emoji_preferred_skin_tone.clone()),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "inline_emoji_picker", "enabled"],
        Value::Bool(snapshot.composer.input_inline_emoji_picker_enabled),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "inline_room_picker", "enabled"],
        Value::Bool(snapshot.composer.input_inline_room_picker_enabled),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "inline_user_picker", "enabled"],
        Value::Bool(snapshot.composer.input_inline_user_picker_enabled),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "selection_formatting_toolbar", "enabled"],
        Value::Bool(snapshot.composer.input_selection_formatting_toolbar_enabled),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "transcription", "enabled"],
        Value::Bool(snapshot.composer.input_transcription_enabled),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "spellcheck", "enabled"],
        Value::Bool(snapshot.composer.input_spellcheck_enabled),
    );
    yaml::set_value(
        &mut root,
        &["composer", "input", "spellcheck", "languages"],
        Value::Sequence(
            snapshot
                .composer
                .input_spellcheck_languages
                .iter()
                .map(|value| Value::String(value.clone()))
                .collect(),
        ),
    );
    yaml::set_value(
        &mut root,
        &["composer", "attachments", "strip_image_metadata"],
        Value::Bool(snapshot.composer.attachments_strip_image_metadata),
    );
    yaml::set_value(
        &mut root,
        &["composer", "typing", "send", "global"],
        Value::Bool(snapshot.composer.typing_send_global),
    );
    yaml::set_value(
        &mut root,
        &["composer", "typing", "send", "by_room"],
        tree::bool_map(&snapshot.composer.typing_send_by_room),
    );

    yaml::serialize_yaml(&root)
}

pub(super) fn load_config_snapshot(config_text: &str) -> LoadedConfig {
    let mut root = yaml::parse_root(config_text);
    let source_version = yaml::read_schema_version(&root, &CONFIG_SCHEMA_VERSION_PATH);
    let mut had_future_version = false;
    let had_unsupported_path = false;
    let migrated_version;
    let should_write_back;

    if source_version > CURRENT_CONFIG_SCHEMA_VERSION {
        had_future_version = true;
        migrated_version = source_version;
        should_write_back = false;
    } else {
        migrated_version = CURRENT_CONFIG_SCHEMA_VERSION;
        yaml::set_value(
            &mut root,
            &CONFIG_SCHEMA_VERSION_PATH,
            yaml::number_value(CURRENT_CONFIG_SCHEMA_VERSION),
        );
        should_write_back = source_version != migrated_version;
    }

    let config = super::parse_config_root(&root);

    LoadedConfig {
        config,
        source_exists: !config_text.is_empty(),
        source_version,
        migrated_version,
        had_future_version,
        had_unsupported_path,
        should_write_back,
        serialized_yaml: yaml::serialize_yaml(&root),
    }
}
