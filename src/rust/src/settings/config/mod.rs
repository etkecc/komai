// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

mod bridge;
pub(crate) mod defaults;
mod model;
mod tree;
mod tokens;

use crate::ffi::SettingsConfigSnapshot;
use crate::settings::yaml;

use super::storage;
use tokens::StorageToken;

pub use model::{
    Config, ConfigCalls, ConfigCallsAudio, ConfigCallsDevices, ConfigCallsElement,
    ConfigCallsLegacy, ConfigCallsRelay, ConfigCallsScreenshare, ConfigComposer,
    ConfigComposerTypingSend,
    ConfigDesktop, ConfigDesktopAttention, ConfigDesktopAttentionToggle, ConfigDesktopNotifications,
    ConfigDesktopSystemTray, ConfigDesktopWindowFocusBlur,
    ConfigIntegrations, ConfigIntegrationsTranscription, ConfigIntegrationsTranscriptionOverrides,
    ConfigNetwork, ConfigNetworkEncryption, ConfigSecrets, ConfigNavigation,
    ConfigNavigationCommunities, ConfigNavigationRoomList, ConfigNavigationTabs, ConfigTimeline,
    ConfigTimelineDateDividers, ConfigTimelineFormatted, ConfigTimelineHiddenEvents, ConfigTimelineMedia,
    ConfigTimelineMessageActions, ConfigTimelineMessages, ConfigTimelineRoomHeader,
    ConfigTimelineThreads, ConfigTimelineThreadsCollapseReplies,
    ConfigTimelineMessagesLayout, ConfigTimelineReadReceipts, ConfigTimelineTyping, ConfigUi,
    ConfigUiAvatars, ConfigUiFont, ConfigUiLayout, ConfigUiMotion, ConfigUiScale,
    ConfigUiTheme, LoadedConfig,
};
pub(crate) use model::CURRENT_CONFIG_SCHEMA_VERSION;
pub use tokens::{
    ConfigComposerEmojiPreferredGenderToken, ConfigComposerEmojiPreferredSkinToneToken,
    ConfigComposerInputAutoReplaceEmojiToken, ConfigComposerInputSendKeyToken,
    ConfigIntegrationsDbusApiAccessToken, ConfigIntegrationsTranscriptionProviderToken,
    ConfigDesktopSystemTrayIconStyleToken,
    ConfigNetworkPresenceStatusPolicyToken,
    ConfigNotificationsMessageContentPolicyToken, ConfigSecretsProviderToken,
    ConfigNavigationRoomListLastMessagePreviewToken, ConfigNavigationRoomListSortToken,
    ConfigNavigationRoomListOpeningPolicyToken,
    ConfigNavigationTabsLabelDisplayToken, ConfigNavigationTabsPinButtonVisibilityToken,
    ConfigTimelineMediaImageDisplayToken,
    ConfigTimelineMessageActionsActivationPolicyToken,
    ConfigTimelineMessagesLayoutAvatarSizeToken, ConfigTimelineMessagesPositioningToken,
    ConfigTimelineMessagesSenderUsernameToken, ConfigTimelineMessagesStyleToken,
    ConfigTimelineRoomHeaderButtonLabelsToken,
    ConfigTimelineUserColorCodingPolicyToken, ConfigUiDefaultAvatarStyleToken,
    ConfigUiLayoutDensityToken, ConfigUiScrollbarPolicyToken, ConfigUiThemeModeToken,
};

const UI_SCALE_FACTOR_PATH: [&str; 3] = ["ui", "scale", "factor"];
const UI_THEME_SLUG_PATH: [&str; 3] = ["ui", "theme", "slug"];
const UI_THEME_MODE_PATH: [&str; 3] = ["ui", "theme", "mode"];
const UI_FONT_FAMILY_PATH: [&str; 3] = ["ui", "font", "family"];
const UI_FONT_EMOJI_FAMILY_PATH: [&str; 3] = ["ui", "font", "emoji_family"];
const UI_FONT_SIZE_PT_PATH: [&str; 3] = ["ui", "font", "size_pt"];
const UI_MOTION_ANIMATIONS_ENABLED_PATH: [&str; 3] = ["ui", "motion", "enable_animations"];
const UI_LAYOUT_DENSITY_PATH: [&str; 3] = ["ui", "layout", "density"];
const UI_AVATARS_CIRCULAR_PATH: [&str; 3] = ["ui", "avatars", "circular"];
const UI_AVATARS_DEFAULT_AVATAR_STYLE_PATH: [&str; 3] = ["ui", "avatars", "default_avatar_style"];
const UI_SCROLLBAR_POLICY_PATH: [&str; 2] = ["ui", "scrollbar_policy"];
const UI_LANGUAGE_PATH: [&str; 2] = ["ui", "language"];
const NAVIGATION_ROOM_LIST_SHOW_LAST_MESSAGE_TIME_PATH: [&str; 3] =
    ["navigation", "room_list", "show_last_message_timestamp"];
const NAVIGATION_ROOM_LIST_LAST_MESSAGE_PREVIEW_PATH: [&str; 3] =
    ["navigation", "room_list", "last_message_preview"];
const NAVIGATION_ROOM_LIST_SHOW_UNREAD_INDICATORS_PATH: [&str; 3] =
    ["navigation", "room_list", "show_unread_indicators"];
const NAVIGATION_ROOM_LIST_SORT_PATH: [&str; 3] = ["navigation", "room_list", "sort"];
const NAVIGATION_COMMUNITIES_SHOW_UNREAD_INDICATORS_PATH: [&str; 3] =
    ["navigation", "communities", "show_unread_indicators"];
const NAVIGATION_ROOM_LIST_OPENING_POLICY_PATH: [&str; 3] =
    ["navigation", "room_list", "opening_policy"];
const NAVIGATION_COMMUNITIES_FILTER_FAVOURITES_PATH: [&str; 4] =
    ["navigation", "communities", "filters", "favourites"];
const NAVIGATION_COMMUNITIES_FILTER_PEOPLE_PATH: [&str; 4] =
    ["navigation", "communities", "filters", "people"];
const NAVIGATION_COMMUNITIES_FILTER_BOTS_PATH: [&str; 4] =
    ["navigation", "communities", "filters", "bots"];
const NAVIGATION_COMMUNITIES_FILTER_GROUPS_PATH: [&str; 4] =
    ["navigation", "communities", "filters", "groups"];
const NAVIGATION_COMMUNITIES_FILTER_SERVER_NOTICES_PATH: [&str; 4] =
    ["navigation", "communities", "filters", "server_notices"];
const NAVIGATION_COMMUNITIES_FILTER_LOW_PRIORITY_PATH: [&str; 4] =
    ["navigation", "communities", "filters", "low_priority"];
const NAVIGATION_TABS_AUTO_HIDE_WITH_SINGLE_TAB_PATH: [&str; 3] =
    ["navigation", "tabs", "auto_hide_with_single_tab"];
const NAVIGATION_TABS_SHOW_PIN_BUTTON_PATH: [&str; 3] =
    ["navigation", "tabs", "show_pin_button"];
const NAVIGATION_TABS_PINNED_TAB_LABEL_PATH: [&str; 3] =
    ["navigation", "tabs", "pinned_tab_label"];
const NAVIGATION_TABS_TAB_LABEL_PATH: [&str; 3] = ["navigation", "tabs", "tab_label"];
const NAVIGATION_TABS_PREFERRED_WIDTH_PX_PATH: [&str; 3] =
    ["navigation", "tabs", "preferred_width_px"];
const NAVIGATION_TABS_MINIMUM_WIDTH_PX_PATH: [&str; 3] =
    ["navigation", "tabs", "minimum_width_px"];
const NAVIGATION_TABS_MAX_PRE_RENDERED_TIMELINES_PATH: [&str; 3] =
    ["navigation", "tabs", "max_recently_closed_timelines"];
const TIMELINE_MESSAGES_STYLE_PATH: [&str; 3] = ["timeline", "messages", "style"];
const TIMELINE_MESSAGES_LAYOUT_POSITIONING_PATH: [&str; 4] =
    ["timeline", "messages", "layout", "positioning"];
const TIMELINE_USER_COLOR_CODING_POLICY_PATH: [&str; 2] =
    ["timeline", "user_color_coding_policy"];
const TIMELINE_MESSAGES_LAYOUT_AVATAR_SIZE_PATH: [&str; 4] =
    ["timeline", "messages", "layout", "avatar_size"];
const TIMELINE_MESSAGES_LAYOUT_SHOW_OWN_AVATAR_PATH: [&str; 4] =
    ["timeline", "messages", "layout", "show_own_avatar"];
const TIMELINE_MESSAGES_LAYOUT_MAX_WIDTH_PERCENT_PATH: [&str; 4] =
    ["timeline", "messages", "layout", "max_width_percent"];
const TIMELINE_MESSAGES_LAYOUT_ADAPTIVE_POSITIONING_BREAKPOINT_PX_PATH: [&str; 4] =
    ["timeline", "messages", "layout", "adaptive_positioning_breakpoint_px"];
const TIMELINE_MESSAGES_SENDER_USERNAME_PATH: [&str; 3] =
    ["timeline", "messages", "sender_username"];
const TIMELINE_MESSAGES_EMOJI_ONLY_ENLARGE_PATH: [&str; 3] =
    ["timeline", "messages", "emoji_only_enlarge"];
const TIMELINE_MESSAGES_HOVER_HIGHLIGHT_PATH: [&str; 3] =
    ["timeline", "messages", "hover_highlight"];
const TIMELINE_MESSAGES_DRAG_SELECT_PATH: [&str; 3] =
    ["timeline", "messages", "drag_select"];
const TIMELINE_FORMATTED_CODE_SYNTAX_HIGHLIGHTING_PATH: [&str; 3] =
    ["timeline", "formatted", "code_syntax_highlighting"];
const TIMELINE_TYPING_SHOW_ENABLED_PATH: [&str; 4] =
    ["timeline", "typing", "show", "enabled"];
/// Pre-v3 path for the timeline read-receipts global toggle (a bool leaf
/// at `timeline.read_receipts.enabled`). Read for compat-fallback only;
/// v3 stores the global at `timeline.read_receipts.global` (sibling of
/// the new `by_room` map). See `migrations.md` for the v2→v3 step.
const TIMELINE_READ_RECEIPTS_ENABLED_PATH_LEGACY: [&str; 3] =
    ["timeline", "read_receipts", "enabled"];
const TIMELINE_READ_RECEIPTS_GLOBAL_PATH: [&str; 3] =
    ["timeline", "read_receipts", "global"];
const TIMELINE_READ_RECEIPTS_BY_ROOM_PATH: [&str; 3] =
    ["timeline", "read_receipts", "by_room"];
const TIMELINE_MESSAGE_ACTIONS_ACTIVATION_POLICY_PATH: [&str; 4] =
    ["timeline", "messages", "actions", "activation_policy"];
const TIMELINE_MESSAGE_ACTIONS_PINNED_REACTIONS_PATH: [&str; 4] =
    ["timeline", "messages", "actions", "pinned_reactions"];
const TIMELINE_MEDIA_EFFECTS_ENABLED_PATH: [&str; 4] =
    ["timeline", "media", "effects", "enabled"];
const TIMELINE_MEDIA_ANIMATE_ON_HOVER_PATH: [&str; 3] =
    ["timeline", "media", "animate_on_hover"];
const TIMELINE_MEDIA_IMAGE_DISPLAY_PATH: [&str; 3] =
    ["timeline", "media", "image_display"];
const TIMELINE_MEDIA_OPEN_IMAGES_EXTERNAL_PATH: [&str; 3] =
    ["timeline", "media", "open_images_external"];
const TIMELINE_MEDIA_OPEN_VIDEOS_EXTERNAL_PATH: [&str; 3] =
    ["timeline", "media", "open_videos_external"];
const TIMELINE_MEDIA_AUTOPLAY_GIF_VIDEOS_PATH: [&str; 3] =
    ["timeline", "media", "autoplay_gif_videos"];
const TIMELINE_MEDIA_OPEN_AUDIO_EXTERNAL_PATH: [&str; 3] =
    ["timeline", "media", "open_audio_external"];
const TIMELINE_MEDIA_DEFAULT_AUDIO_PLAYBACK_SPEED_PATH: [&str; 3] =
    ["timeline", "media", "default_audio_playback_speed"];
const TIMELINE_THREADS_COLLAPSE_REPLIES_GLOBAL_PATH: [&str; 4] =
    ["timeline", "threads", "collapse_replies", "global"];
const TIMELINE_DATE_DIVIDERS_ENABLED_PATH: [&str; 3] =
    ["timeline", "date_dividers", "enabled"];
const TIMELINE_ROOM_HEADER_BUTTON_LABELS_PATH: [&str; 3] =
    ["timeline", "room_header", "button_labels"];
const TIMELINE_THREADS_COLLAPSE_REPLIES_BY_ROOM_PATH: [&str; 4] =
    ["timeline", "threads", "collapse_replies", "by_room"];
const HIDDEN_EVENTS_GLOBAL_PATH: [&str; 3] = ["timeline", "hidden_events", "global"];
const HIDDEN_EVENTS_BY_ROOM_PATH: [&str; 3] = ["timeline", "hidden_events", "by_room"];
const SECRETS_PROVIDER_PATH: [&str; 2] = ["secrets", "provider"];
const DESKTOP_NOTIFICATIONS_ENABLED_PATH: [&str; 3] = ["desktop", "notifications", "enabled"];
const DESKTOP_NOTIFICATIONS_ATTENTION_ON_INCOMING_PATH: [&str; 3] =
    ["desktop", "notifications", "attention_on_incoming"];
const DESKTOP_NOTIFICATIONS_MESSAGE_CONTENT_POLICY_PATH: [&str; 3] =
    ["desktop", "notifications", "message_content_policy"];
const DESKTOP_ATTENTION_WINDOW_TITLE_ENABLED_PATH: [&str; 4] =
    ["desktop", "attention", "window_title", "enabled"];
const DESKTOP_ATTENTION_APP_BADGE_ENABLED_PATH: [&str; 4] =
    ["desktop", "attention", "app_badge", "enabled"];
const DESKTOP_SYSTEM_TRAY_ENABLED_PATH: [&str; 3] = ["desktop", "system_tray", "enabled"];
const DESKTOP_SYSTEM_TRAY_AUTOSTART_PATH: [&str; 3] = ["desktop", "system_tray", "autostart"];
const DESKTOP_SYSTEM_TRAY_ICON_STYLE_PATH: [&str; 3] =
    ["desktop", "system_tray", "icon_style"];
const DESKTOP_WINDOW_FOCUS_BLUR_ENABLED_PATH: [&str; 3] =
    ["desktop", "window_focus_blur", "enabled"];
const DESKTOP_WINDOW_FOCUS_BLUR_DELAY_SECONDS_PATH: [&str; 3] =
    ["desktop", "window_focus_blur", "delay_seconds"];
const NETWORK_ENCRYPTION_ONLY_VERIFIED_USERS_PATH: [&str; 3] =
    ["network", "encryption", "only_verified_users"];
const NETWORK_ENCRYPTION_SHARE_WITH_TRUSTED_PATH: [&str; 3] =
    ["network", "encryption", "share_with_trusted"];
const NETWORK_ENCRYPTION_KEY_BACKUP_PATH: [&str; 3] =
    ["network", "encryption", "key_backup"];
const CALLS_LEGACY_ENABLED_PATH: [&str; 3] = ["calls", "legacy", "enabled"];
const CALLS_ELEMENT_ENABLED_PATH: [&str; 3] = ["calls", "element", "enabled"];
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
const NETWORK_PRESENCE_STATUS_POLICY_PATH: [&str; 3] = ["network", "presence", "status_policy"];
const NETWORK_TLS_ENABLE_CERTIFICATE_VALIDATION_PATH: [&str; 3] =
    ["network", "tls", "enable_certificate_validation"];
const NETWORK_MRS_ENABLED_PATH: [&str; 3] = ["network", "mrs", "enabled"];
const NETWORK_MRS_SERVER_NAME_PATH: [&str; 3] = ["network", "mrs", "server_name"];
const NETWORK_HTTP3_ENABLED_PATH: [&str; 3] = ["network", "http3", "enabled"];
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
const COMPOSER_INPUT_SELECTION_FORMATTING_TOOLBAR_ENABLED_PATH: [&str; 4] =
    ["composer", "input", "selection_formatting_toolbar", "enabled"];
const COMPOSER_INPUT_TRANSCRIPTION_ENABLED_PATH: [&str; 4] =
    ["composer", "input", "transcription", "enabled"];
const COMPOSER_INPUT_SPELLCHECK_ENABLED_PATH: [&str; 4] =
    ["composer", "input", "spellcheck", "enabled"];
const COMPOSER_INPUT_SPELLCHECK_LANGUAGES_PATH: [&str; 4] =
    ["composer", "input", "spellcheck", "languages"];
const COMPOSER_ATTACHMENTS_STRIP_IMAGE_METADATA_PATH: [&str; 3] =
    ["composer", "attachments", "strip_image_metadata"];
/// Pre-v2 path for the composer typing-send global toggle (a bool leaf
/// at `composer.typing.send.enabled`). Read for compat-fallback only;
/// v2 stores the global at `composer.typing.send.global` (sibling of
/// the new `by_room` map). See `migrations.md` for the v1→v2 step.
const COMPOSER_TYPING_SEND_ENABLED_PATH_LEGACY: [&str; 4] =
    ["composer", "typing", "send", "enabled"];
const COMPOSER_TYPING_SEND_GLOBAL_PATH: [&str; 4] =
    ["composer", "typing", "send", "global"];
const COMPOSER_TYPING_SEND_BY_ROOM_PATH: [&str; 4] =
    ["composer", "typing", "send", "by_room"];
const INTEGRATIONS_TRANSCRIPTION_PROVIDER_PATH: [&str; 3] =
    ["integrations", "transcription", "provider"];
const INTEGRATIONS_TRANSCRIPTION_API_URL_PATH: [&str; 3] =
    ["integrations", "transcription", "api_url"];
const INTEGRATIONS_TRANSCRIPTION_MODEL_PATH: [&str; 3] =
    ["integrations", "transcription", "model"];
const INTEGRATIONS_TRANSCRIPTION_LANGUAGE_PATH: [&str; 3] =
    ["integrations", "transcription", "language"];
const INTEGRATIONS_TRANSCRIPTION_PROMPT_PATH: [&str; 3] =
    ["integrations", "transcription", "prompt"];
const INTEGRATIONS_TRANSCRIPTION_BY_ROOM_PATH: [&str; 3] =
    ["integrations", "transcription", "by_room"];

// Per-room override sub-keys (relative to a room mapping under `by_room`).
// Note: `api_key` is intentionally absent — api keys live in the secrets
// backend, never in config.yml. See plan §"API key storage".
// `enabled` is also intentionally absent — the master toggle lives under
// `composer.input.transcription.enabled` and has no per-room override.
const INTEGRATIONS_TRANSCRIPTION_OVERRIDE_PROVIDER_KEY: &str = "provider";
const INTEGRATIONS_TRANSCRIPTION_OVERRIDE_API_URL_KEY: &str = "api_url";
const INTEGRATIONS_TRANSCRIPTION_OVERRIDE_MODEL_KEY: &str = "model";
const INTEGRATIONS_TRANSCRIPTION_OVERRIDE_LANGUAGE_KEY: &str = "language";
const INTEGRATIONS_TRANSCRIPTION_OVERRIDE_PROMPT_KEY: &str = "prompt";

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
                mode: yaml::value_at_path(root, &UI_THEME_MODE_PATH)
                    .and_then(parse_optional_storage_token),
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
            layout: ConfigUiLayout {
                density: parse_storage_token(yaml::value_at_path(root, &UI_LAYOUT_DENSITY_PATH)),
            },
            avatars: ConfigUiAvatars {
                circular: yaml::value_at_path(root, &UI_AVATARS_CIRCULAR_PATH)
                    .and_then(parse_scalar_bool),
                default_avatar_style: parse_storage_token(
                    yaml::value_at_path(root, &UI_AVATARS_DEFAULT_AVATAR_STYLE_PATH),
                ),
            },
            scrollbar_policy: parse_storage_token(yaml::value_at_path(root, &UI_SCROLLBAR_POLICY_PATH)),
            language: parse_string(yaml::value_at_path(root, &UI_LANGUAGE_PATH)),
        },
        navigation: ConfigNavigation {
            room_list: ConfigNavigationRoomList {
                show_last_message_time: yaml::value_at_path(
                    root,
                    &NAVIGATION_ROOM_LIST_SHOW_LAST_MESSAGE_TIME_PATH,
                )
                .and_then(parse_scalar_bool),
                last_message_preview: parse_storage_token(yaml::value_at_path(
                    root,
                    &NAVIGATION_ROOM_LIST_LAST_MESSAGE_PREVIEW_PATH,
                )),
                show_unread_indicators: yaml::value_at_path(
                    root,
                    &NAVIGATION_ROOM_LIST_SHOW_UNREAD_INDICATORS_PATH,
                )
                .and_then(parse_scalar_bool),
                sort: parse_storage_token(yaml::value_at_path(root, &NAVIGATION_ROOM_LIST_SORT_PATH)),
                opening_policy: parse_storage_token(yaml::value_at_path(
                    root,
                    &NAVIGATION_ROOM_LIST_OPENING_POLICY_PATH,
                )),
            },
            communities: ConfigNavigationCommunities {
                show_unread_indicators: yaml::value_at_path(
                    root,
                    &NAVIGATION_COMMUNITIES_SHOW_UNREAD_INDICATORS_PATH,
                )
                .and_then(parse_scalar_bool),
                filter_favourites: yaml::value_at_path(
                    root,
                    &NAVIGATION_COMMUNITIES_FILTER_FAVOURITES_PATH,
                )
                .and_then(parse_scalar_bool),
                filter_people: yaml::value_at_path(root, &NAVIGATION_COMMUNITIES_FILTER_PEOPLE_PATH)
                    .and_then(parse_scalar_bool),
                filter_bots: yaml::value_at_path(root, &NAVIGATION_COMMUNITIES_FILTER_BOTS_PATH)
                    .and_then(parse_scalar_bool),
                filter_groups: yaml::value_at_path(root, &NAVIGATION_COMMUNITIES_FILTER_GROUPS_PATH)
                    .and_then(parse_scalar_bool),
                filter_server_notices: yaml::value_at_path(
                    root,
                    &NAVIGATION_COMMUNITIES_FILTER_SERVER_NOTICES_PATH,
                )
                .and_then(parse_scalar_bool),
                filter_low_priority: yaml::value_at_path(
                    root,
                    &NAVIGATION_COMMUNITIES_FILTER_LOW_PRIORITY_PATH,
                )
                .and_then(parse_scalar_bool),
            },
            tabs: ConfigNavigationTabs {
                auto_hide_with_single_tab: yaml::value_at_path(
                    root,
                    &NAVIGATION_TABS_AUTO_HIDE_WITH_SINGLE_TAB_PATH,
                )
                .and_then(parse_scalar_bool),
                show_pin_button: parse_storage_token(yaml::value_at_path(
                    root,
                    &NAVIGATION_TABS_SHOW_PIN_BUTTON_PATH,
                )),
                pinned_tab_label: parse_storage_token(yaml::value_at_path(
                    root,
                    &NAVIGATION_TABS_PINNED_TAB_LABEL_PATH,
                )),
                tab_label: parse_storage_token_or(
                    yaml::value_at_path(root, &NAVIGATION_TABS_TAB_LABEL_PATH),
                    ConfigNavigationTabsLabelDisplayToken::AvatarAndLabel,
                ),
                preferred_width_px: yaml::value_at_path(
                    root,
                    &NAVIGATION_TABS_PREFERRED_WIDTH_PX_PATH,
                )
                .and_then(parse_scalar_i32),
                minimum_width_px: yaml::value_at_path(
                    root,
                    &NAVIGATION_TABS_MINIMUM_WIDTH_PX_PATH,
                )
                .and_then(parse_scalar_i32),
                max_recently_closed_timelines: yaml::value_at_path(
                    root,
                    &NAVIGATION_TABS_MAX_PRE_RENDERED_TIMELINES_PATH,
                )
                .and_then(parse_scalar_i32),
            },
        },
        timeline: ConfigTimeline {
            messages: ConfigTimelineMessages {
                style: parse_storage_token(yaml::value_at_path(root, &TIMELINE_MESSAGES_STYLE_PATH)),
                layout: ConfigTimelineMessagesLayout {
                    positioning: parse_storage_token(yaml::value_at_path(
                        root,
                        &TIMELINE_MESSAGES_LAYOUT_POSITIONING_PATH,
                    )),
                    avatar_size: parse_storage_token(yaml::value_at_path(
                        root,
                        &TIMELINE_MESSAGES_LAYOUT_AVATAR_SIZE_PATH,
                    )),
                    show_own_avatar: yaml::value_at_path(
                        root,
                        &TIMELINE_MESSAGES_LAYOUT_SHOW_OWN_AVATAR_PATH,
                    )
                    .and_then(parse_scalar_bool),
                    max_width_percent: yaml::value_at_path(
                        root,
                        &TIMELINE_MESSAGES_LAYOUT_MAX_WIDTH_PERCENT_PATH,
                    )
                    .and_then(parse_scalar_i32),
                    adaptive_positioning_breakpoint_px: yaml::value_at_path(
                        root,
                        &TIMELINE_MESSAGES_LAYOUT_ADAPTIVE_POSITIONING_BREAKPOINT_PX_PATH,
                    )
                    .and_then(parse_scalar_i32),
                },
                sender_username: parse_storage_token(yaml::value_at_path(
                    root,
                    &TIMELINE_MESSAGES_SENDER_USERNAME_PATH,
                )),
                emoji_only_enlarge: yaml::value_at_path(
                    root,
                    &TIMELINE_MESSAGES_EMOJI_ONLY_ENLARGE_PATH,
                )
                .and_then(parse_scalar_bool),
                hover_highlight: yaml::value_at_path(
                    root,
                    &TIMELINE_MESSAGES_HOVER_HIGHLIGHT_PATH,
                )
                .and_then(parse_scalar_bool),
                drag_select: yaml::value_at_path(
                    root,
                    &TIMELINE_MESSAGES_DRAG_SELECT_PATH,
                )
                .and_then(parse_scalar_bool),
            },
            user_color_coding_policy: parse_storage_token(yaml::value_at_path(
                root,
                &TIMELINE_USER_COLOR_CODING_POLICY_PATH,
            )),
            formatted: ConfigTimelineFormatted {
                code_syntax_highlighting: yaml::value_at_path(
                    root,
                    &TIMELINE_FORMATTED_CODE_SYNTAX_HIGHLIGHTING_PATH,
                )
                .and_then(parse_scalar_bool),
            },
            typing: ConfigTimelineTyping {
                show_enabled: yaml::value_at_path(root, &TIMELINE_TYPING_SHOW_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
            },
            read_receipts: ConfigTimelineReadReceipts {
                // Prefer the v3 path; fall back to the legacy `enabled` leaf
                // for configs written before the v2→v3 migration step ran.
                global: yaml::value_at_path(root, &TIMELINE_READ_RECEIPTS_GLOBAL_PATH)
                    .and_then(parse_scalar_bool)
                    .or_else(|| {
                        yaml::value_at_path(root, &TIMELINE_READ_RECEIPTS_ENABLED_PATH_LEGACY)
                            .and_then(parse_scalar_bool)
                    }),
                by_room: parse_bool_map(yaml::value_at_path(
                    root,
                    &TIMELINE_READ_RECEIPTS_BY_ROOM_PATH,
                )),
            },
            message_actions: ConfigTimelineMessageActions {
                activation_policy: parse_storage_token(yaml::value_at_path(
                    root,
                    &TIMELINE_MESSAGE_ACTIONS_ACTIVATION_POLICY_PATH,
                )),
                pinned_reactions: parse_string(yaml::value_at_path(
                    root,
                    &TIMELINE_MESSAGE_ACTIONS_PINNED_REACTIONS_PATH,
                )),
            },
            media: ConfigTimelineMedia {
                effects_enabled: yaml::value_at_path(root, &TIMELINE_MEDIA_EFFECTS_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
                animate_on_hover: yaml::value_at_path(root, &TIMELINE_MEDIA_ANIMATE_ON_HOVER_PATH)
                    .and_then(parse_scalar_bool),
                image_display: parse_storage_token(yaml::value_at_path(
                    root,
                    &TIMELINE_MEDIA_IMAGE_DISPLAY_PATH,
                )),
                open_images_external: yaml::value_at_path(
                    root,
                    &TIMELINE_MEDIA_OPEN_IMAGES_EXTERNAL_PATH,
                )
                .and_then(parse_scalar_bool),
                open_videos_external: yaml::value_at_path(
                    root,
                    &TIMELINE_MEDIA_OPEN_VIDEOS_EXTERNAL_PATH,
                )
                .and_then(parse_scalar_bool),
                autoplay_gif_videos: yaml::value_at_path(
                    root,
                    &TIMELINE_MEDIA_AUTOPLAY_GIF_VIDEOS_PATH,
                )
                .and_then(parse_scalar_bool),
                open_audio_external: yaml::value_at_path(
                    root,
                    &TIMELINE_MEDIA_OPEN_AUDIO_EXTERNAL_PATH,
                )
                .and_then(parse_scalar_bool),
                default_audio_playback_speed: yaml::value_at_path(
                    root,
                    &TIMELINE_MEDIA_DEFAULT_AUDIO_PLAYBACK_SPEED_PATH,
                )
                .and_then(parse_scalar_f64),
            },
            hidden_events: ConfigTimelineHiddenEvents {
                global: parse_string_list(yaml::value_at_path(root, &HIDDEN_EVENTS_GLOBAL_PATH)),
                by_room: parse_string_list_map(yaml::value_at_path(root, &HIDDEN_EVENTS_BY_ROOM_PATH)),
            },
            threads: ConfigTimelineThreads {
                collapse_replies: ConfigTimelineThreadsCollapseReplies {
                    global: yaml::value_at_path(
                        root,
                        &TIMELINE_THREADS_COLLAPSE_REPLIES_GLOBAL_PATH,
                    )
                    .and_then(parse_scalar_bool),
                    by_room: parse_bool_map(yaml::value_at_path(
                        root,
                        &TIMELINE_THREADS_COLLAPSE_REPLIES_BY_ROOM_PATH,
                    )),
                },
            },
            date_dividers: ConfigTimelineDateDividers {
                enabled: yaml::value_at_path(root, &TIMELINE_DATE_DIVIDERS_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
            },
            room_header: ConfigTimelineRoomHeader {
                button_labels: parse_storage_token(yaml::value_at_path(
                    root,
                    &TIMELINE_ROOM_HEADER_BUTTON_LABELS_PATH,
                )),
            },
        },
        secrets: ConfigSecrets {
            provider: parse_storage_token(yaml::value_at_path(root, &SECRETS_PROVIDER_PATH)),
        },
        desktop: ConfigDesktop {
            notifications: ConfigDesktopNotifications {
                enabled: yaml::value_at_path(root, &DESKTOP_NOTIFICATIONS_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
                attention_on_incoming: yaml::value_at_path(
                    root,
                    &DESKTOP_NOTIFICATIONS_ATTENTION_ON_INCOMING_PATH,
                )
                .and_then(parse_scalar_bool),
                message_content_policy: parse_storage_token(yaml::value_at_path(
                    root,
                    &DESKTOP_NOTIFICATIONS_MESSAGE_CONTENT_POLICY_PATH,
                )),
            },
            attention: ConfigDesktopAttention {
                window_title: ConfigDesktopAttentionToggle {
                    enabled: yaml::value_at_path(root, &DESKTOP_ATTENTION_WINDOW_TITLE_ENABLED_PATH)
                        .and_then(parse_scalar_bool),
                },
                app_badge: ConfigDesktopAttentionToggle {
                    enabled: yaml::value_at_path(root, &DESKTOP_ATTENTION_APP_BADGE_ENABLED_PATH)
                        .and_then(parse_scalar_bool),
                },
            },
            system_tray: ConfigDesktopSystemTray {
                enabled: yaml::value_at_path(root, &DESKTOP_SYSTEM_TRAY_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
                autostart: yaml::value_at_path(root, &DESKTOP_SYSTEM_TRAY_AUTOSTART_PATH)
                    .and_then(parse_scalar_bool),
                icon_style: parse_storage_token(yaml::value_at_path(
                    root,
                    &DESKTOP_SYSTEM_TRAY_ICON_STYLE_PATH,
                )),
            },
            window_focus_blur: ConfigDesktopWindowFocusBlur {
                enabled: yaml::value_at_path(root, &DESKTOP_WINDOW_FOCUS_BLUR_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
                delay_seconds: yaml::value_at_path(
                    root,
                    &DESKTOP_WINDOW_FOCUS_BLUR_DELAY_SECONDS_PATH,
                )
                .and_then(parse_scalar_i32),
            },
        },
        calls: ConfigCalls {
            legacy: ConfigCallsLegacy {
                enabled: yaml::value_at_path(root, &CALLS_LEGACY_ENABLED_PATH)
                    .and_then(parse_scalar_bool),
            },
            element: ConfigCallsElement {
                enabled: yaml::value_at_path(root, &CALLS_ELEMENT_ENABLED_PATH)
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
        network: ConfigNetwork {
            encryption: ConfigNetworkEncryption {
                only_verified_users: yaml::value_at_path(
                    root,
                    &NETWORK_ENCRYPTION_ONLY_VERIFIED_USERS_PATH,
                )
                .and_then(parse_scalar_bool),
                share_with_trusted: yaml::value_at_path(
                    root,
                    &NETWORK_ENCRYPTION_SHARE_WITH_TRUSTED_PATH,
                )
                .and_then(parse_scalar_bool),
                key_backup: yaml::value_at_path(root, &NETWORK_ENCRYPTION_KEY_BACKUP_PATH)
                    .and_then(parse_scalar_bool),
            },
            presence_status_policy: parse_storage_token(yaml::value_at_path(
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
            dbus_api_access: parse_storage_token(yaml::value_at_path(
                root,
                &INTEGRATIONS_DBUS_API_ACCESS_PATH,
            )),
            browser_command: parse_string(yaml::value_at_path(root, &INTEGRATIONS_BROWSER_COMMAND_PATH)),
            // For a fresh profile (no transcription keys in YAML), default
            // `provider` and `api_url` to OpenAI cloud so the resolver lands
            // in a "ready, just needs an api_key" state instead of "not
            // configured". An explicitly-set empty string (Some("")) — e.g.
            // after the user picks Hosting → Other and clears the URL — is
            // preserved as-is so the resolver correctly reports "not ready"
            // until they enter a custom URL.
            transcription: ConfigIntegrationsTranscription {
                provider: yaml::value_at_path(root, &INTEGRATIONS_TRANSCRIPTION_PROVIDER_PATH)
                    .and_then(parse_optional_storage_token)
                    .or(Some(ConfigIntegrationsTranscriptionProviderToken::OpenaiRealtime)),
                api_url: yaml::value_at_path(root, &INTEGRATIONS_TRANSCRIPTION_API_URL_PATH)
                    .and_then(parse_optional_string)
                    .or_else(|| Some("https://api.openai.com/v1".to_owned())),
                api_key: None, // Populated from the secrets backend, never from config.yml.
                model: yaml::value_at_path(root, &INTEGRATIONS_TRANSCRIPTION_MODEL_PATH)
                    .and_then(parse_optional_string),
                language: yaml::value_at_path(root, &INTEGRATIONS_TRANSCRIPTION_LANGUAGE_PATH)
                    .and_then(parse_optional_string),
                prompt: yaml::value_at_path(root, &INTEGRATIONS_TRANSCRIPTION_PROMPT_PATH)
                    .and_then(parse_optional_string),
                by_room: parse_integrations_transcription_overrides_map(yaml::value_at_path(
                    root,
                    &INTEGRATIONS_TRANSCRIPTION_BY_ROOM_PATH,
                )),
            },
        },
        composer: ConfigComposer {
            input_markdown_to_html_enabled: yaml::value_at_path(
                root,
                &COMPOSER_INPUT_MARKDOWN_TO_HTML_ENABLED_PATH,
            )
            .and_then(parse_scalar_bool),
            input_send_key: parse_storage_token(yaml::value_at_path(
                root,
                &COMPOSER_INPUT_SEND_KEY_PATH,
            )),
            input_auto_replace_emoji: parse_storage_token(yaml::value_at_path(
                root,
                &COMPOSER_INPUT_AUTO_REPLACE_EMOJI_PATH,
            )),
            input_emoji_preferred_gender: parse_storage_token(yaml::value_at_path(
                root,
                &COMPOSER_INPUT_EMOJI_PREFERRED_GENDER_PATH,
            )),
            input_emoji_preferred_skin_tone: parse_storage_token(yaml::value_at_path(
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
            input_selection_formatting_toolbar_enabled: yaml::value_at_path(
                root,
                &COMPOSER_INPUT_SELECTION_FORMATTING_TOOLBAR_ENABLED_PATH,
            )
            .and_then(parse_scalar_bool),
            input_transcription_enabled: yaml::value_at_path(
                root,
                &COMPOSER_INPUT_TRANSCRIPTION_ENABLED_PATH,
            )
            .and_then(parse_scalar_bool),
            input_spellcheck_enabled: yaml::value_at_path(
                root,
                &COMPOSER_INPUT_SPELLCHECK_ENABLED_PATH,
            )
            .and_then(parse_scalar_bool),
            input_spellcheck_languages: parse_string_list(yaml::value_at_path(
                root,
                &COMPOSER_INPUT_SPELLCHECK_LANGUAGES_PATH,
            )),
            attachments_strip_image_metadata: yaml::value_at_path(
                root,
                &COMPOSER_ATTACHMENTS_STRIP_IMAGE_METADATA_PATH,
            )
            .and_then(parse_scalar_bool),
            typing_send: model::ConfigComposerTypingSend {
                // Prefer the v2 path; fall back to the legacy `enabled` leaf
                // for configs written before the v1→v2 migration step ran.
                global: yaml::value_at_path(root, &COMPOSER_TYPING_SEND_GLOBAL_PATH)
                    .and_then(parse_scalar_bool)
                    .or_else(|| {
                        yaml::value_at_path(root, &COMPOSER_TYPING_SEND_ENABLED_PATH_LEGACY)
                            .and_then(parse_scalar_bool)
                    }),
                by_room: parse_bool_map(yaml::value_at_path(
                    root,
                    &COMPOSER_TYPING_SEND_BY_ROOM_PATH,
                )),
            },
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

fn parse_storage_token<T: StorageToken>(value: Option<&serde_yaml_ng::Value>) -> T {
    T::from_storage_str(&parse_string(value))
}

fn parse_storage_token_or<T: StorageToken>(value: Option<&serde_yaml_ng::Value>, default: T) -> T {
    let s = parse_string(value);
    if s.is_empty() {
        default
    } else {
        T::from_storage_str(&s)
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

fn parse_bool_map(
    value: Option<&serde_yaml_ng::Value>,
) -> std::collections::BTreeMap<String, bool> {
    let Some(serde_yaml_ng::Value::Mapping(mapping)) = value else {
        return std::collections::BTreeMap::new();
    };

    let mut result = std::collections::BTreeMap::new();
    for (key, value) in mapping {
        let serde_yaml_ng::Value::String(key) = key else {
            continue;
        };
        let serde_yaml_ng::Value::Bool(value) = value else {
            continue;
        };
        result.insert(key.clone(), *value);
    }

    result
}

/// Variant of `parse_string` that returns `None` for missing or non-string
/// values (instead of an empty string), so callers can tell "absent" from
/// "deliberately blank".
fn parse_optional_string(value: &serde_yaml_ng::Value) -> Option<String> {
    match value {
        serde_yaml_ng::Value::String(value) => Some(value.trim().to_owned()),
        _ => None,
    }
}

/// Variant of `parse_storage_token` that returns `None` when the value is
/// absent or not a string, so callers can tell "no override set" from
/// "default chosen". Unrecognised string values fall back to the token's
/// default, mirroring `parse_storage_token`.
fn parse_optional_storage_token<T: StorageToken>(value: &serde_yaml_ng::Value) -> Option<T> {
    let serde_yaml_ng::Value::String(string) = value else {
        return None;
    };
    let trimmed = string.trim();
    if trimmed.is_empty() {
        return None;
    }
    Some(T::from_storage_str(trimmed))
}

fn parse_integrations_transcription_overrides_map(
    value: Option<&serde_yaml_ng::Value>,
) -> std::collections::BTreeMap<String, ConfigIntegrationsTranscriptionOverrides> {
    let Some(serde_yaml_ng::Value::Mapping(mapping)) = value else {
        return std::collections::BTreeMap::new();
    };

    let mut result = std::collections::BTreeMap::new();
    for (room_id_key, room_value) in mapping {
        let serde_yaml_ng::Value::String(room_id) = room_id_key else {
            continue;
        };
        let serde_yaml_ng::Value::Mapping(fields) = room_value else {
            continue;
        };

        let mut overrides = ConfigIntegrationsTranscriptionOverrides::default();
        for (field_key, field_value) in fields {
            let serde_yaml_ng::Value::String(field_name) = field_key else {
                continue;
            };
            match field_name.as_str() {
                INTEGRATIONS_TRANSCRIPTION_OVERRIDE_PROVIDER_KEY => {
                    overrides.provider = parse_optional_storage_token(field_value);
                }
                INTEGRATIONS_TRANSCRIPTION_OVERRIDE_API_URL_KEY => {
                    overrides.api_url = parse_optional_string(field_value);
                }
                INTEGRATIONS_TRANSCRIPTION_OVERRIDE_MODEL_KEY => {
                    overrides.model = parse_optional_string(field_value);
                }
                INTEGRATIONS_TRANSCRIPTION_OVERRIDE_LANGUAGE_KEY => {
                    overrides.language = parse_optional_string(field_value);
                }
                INTEGRATIONS_TRANSCRIPTION_OVERRIDE_PROMPT_KEY => {
                    overrides.prompt = parse_optional_string(field_value);
                }
                _ => {}
            }
        }
        result.insert(room_id.clone(), overrides);
    }

    result
}

#[cfg(test)]
mod tests;
