// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

use std::collections::BTreeMap;

use super::tokens::{
    ConfigComposerEmojiPreferredGenderToken, ConfigComposerEmojiPreferredSkinToneToken,
    ConfigComposerInputAutoReplaceEmojiToken, ConfigComposerInputSendKeyToken,
    ConfigDesktopSystemTrayIconStyleToken,
    ConfigIntegrationsDbusApiAccessToken, ConfigIntegrationsTranscriptionProviderToken,
    ConfigNetworkPresenceStatusPolicyToken,
    ConfigNotificationsMessageContentPolicyToken, ConfigSecretsProviderToken,
    ConfigNavigationRoomListLastMessagePreviewToken, ConfigNavigationRoomListSortToken,
    ConfigNavigationRoomListOpeningPolicyToken,
    ConfigNavigationTabsLabelDisplayToken, ConfigNavigationTabsPinButtonVisibilityToken,
    ConfigTimelineMediaImageDisplayToken, ConfigTimelineMessageActionsActivationPolicyToken,
    ConfigTimelineMessagesLayoutAvatarSizeToken, ConfigTimelineMessagesPositioningToken,
    ConfigTimelineMessagesSenderUsernameToken,
    ConfigTimelineMessagesStyleToken, ConfigTimelineRoomHeaderButtonLabelsToken,
    ConfigTimelineUserColorCodingPolicyToken,
    ConfigUiDefaultAvatarStyleToken, ConfigUiLayoutDensityToken, ConfigUiScrollbarPolicyToken,
    ConfigUiThemeModeToken,
};

pub(crate) const CURRENT_CONFIG_SCHEMA_VERSION: i32 = 3;
pub(crate) const CONFIG_SCHEMA_VERSION_PATH: [&str; 2] = ["meta", "settings_schema_version"];

#[derive(Clone, Debug, Default)]
pub struct Config {
    pub ui: ConfigUi,
    pub navigation: ConfigNavigation,
    pub timeline: ConfigTimeline,
    pub secrets: ConfigSecrets,
    pub desktop: ConfigDesktop,
    pub calls: ConfigCalls,
    pub network: ConfigNetwork,
    pub integrations: ConfigIntegrations,
    pub composer: ConfigComposer,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUi {
    pub scale: ConfigUiScale,
    pub theme: ConfigUiTheme,
    pub font: ConfigUiFont,
    pub motion: ConfigUiMotion,
    pub layout: ConfigUiLayout,
    pub avatars: ConfigUiAvatars,
    pub scrollbar_policy: ConfigUiScrollbarPolicyToken,
    pub language: String,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiScale {
    pub factor: Option<f32>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiTheme {
    pub slug: String,
    pub mode: Option<ConfigUiThemeModeToken>,
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
pub struct ConfigUiLayout {
    pub density: ConfigUiLayoutDensityToken,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigUiAvatars {
    pub circular: Option<bool>,
    pub default_avatar_style: ConfigUiDefaultAvatarStyleToken,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigNavigation {
    pub room_list: ConfigNavigationRoomList,
    pub communities: ConfigNavigationCommunities,
    pub tabs: ConfigNavigationTabs,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigNavigationRoomList {
    pub show_last_message_time: Option<bool>,
    pub last_message_preview: ConfigNavigationRoomListLastMessagePreviewToken,
    pub show_unread_indicators: Option<bool>,
    pub sort: ConfigNavigationRoomListSortToken,
    pub opening_policy: ConfigNavigationRoomListOpeningPolicyToken,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigNavigationCommunities {
    pub show_unread_indicators: Option<bool>,
    pub filter_favourites: Option<bool>,
    pub filter_people: Option<bool>,
    pub filter_bots: Option<bool>,
    pub filter_groups: Option<bool>,
    pub filter_server_notices: Option<bool>,
    pub filter_low_priority: Option<bool>,
}

#[derive(Clone, Debug)]
pub struct ConfigNavigationTabs {
    pub auto_hide_with_single_tab: Option<bool>,
    pub show_pin_button: ConfigNavigationTabsPinButtonVisibilityToken,
    pub pinned_tab_label: ConfigNavigationTabsLabelDisplayToken,
    pub tab_label: ConfigNavigationTabsLabelDisplayToken,
    pub preferred_width_px: Option<i32>,
    pub minimum_width_px: Option<i32>,
    pub max_recently_closed_timelines: Option<i32>,
}

impl Default for ConfigNavigationTabs {
    fn default() -> Self {
        Self {
            auto_hide_with_single_tab: None,
            show_pin_button: Default::default(),
            pinned_tab_label: ConfigNavigationTabsLabelDisplayToken::AvatarOnly,
            tab_label: ConfigNavigationTabsLabelDisplayToken::AvatarAndLabel,
            preferred_width_px: None,
            minimum_width_px: None,
            max_recently_closed_timelines: None,
        }
    }
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimeline {
    pub messages: ConfigTimelineMessages,
    pub user_color_coding_policy: ConfigTimelineUserColorCodingPolicyToken,
    pub formatted: ConfigTimelineFormatted,
    pub typing: ConfigTimelineTyping,
    pub read_receipts: ConfigTimelineReadReceipts,
    pub message_actions: ConfigTimelineMessageActions,
    pub media: ConfigTimelineMedia,
    pub hidden_events: ConfigTimelineHiddenEvents,
    pub threads: ConfigTimelineThreads,
    pub date_dividers: ConfigTimelineDateDividers,
    pub room_header: ConfigTimelineRoomHeader,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineRoomHeader {
    pub button_labels: ConfigTimelineRoomHeaderButtonLabelsToken,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineDateDividers {
    pub enabled: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineThreads {
    pub collapse_replies: ConfigTimelineThreadsCollapseReplies,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineThreadsCollapseReplies {
    pub global: Option<bool>,
    pub by_room: BTreeMap<String, bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineMessages {
    pub style: ConfigTimelineMessagesStyleToken,
    pub layout: ConfigTimelineMessagesLayout,
    pub sender_username: ConfigTimelineMessagesSenderUsernameToken,
    pub emoji_only_enlarge: Option<bool>,
    pub hover_highlight: Option<bool>,
    pub drag_select: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineMessagesLayout {
    pub positioning: ConfigTimelineMessagesPositioningToken,
    pub avatar_size: ConfigTimelineMessagesLayoutAvatarSizeToken,
    pub show_own_avatar: Option<bool>,
    pub max_width_percent: Option<i32>,
    pub adaptive_positioning_breakpoint_px: Option<i32>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineFormatted {
    pub code_syntax_highlighting: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineTyping {
    pub show_enabled: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineReadReceipts {
    pub global: Option<bool>,
    pub by_room: BTreeMap<String, bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineMessageActions {
    pub activation_policy: ConfigTimelineMessageActionsActivationPolicyToken,
    pub pinned_reactions: String,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineMedia {
    pub effects_enabled: Option<bool>,
    pub animate_on_hover: Option<bool>,
    pub image_display: ConfigTimelineMediaImageDisplayToken,
    pub open_images_external: Option<bool>,
    pub open_videos_external: Option<bool>,
    pub autoplay_gif_videos: Option<bool>,
    pub open_audio_external: Option<bool>,
    pub default_audio_playback_speed: Option<f64>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigTimelineHiddenEvents {
    pub global: Option<Vec<String>>,
    pub by_room: BTreeMap<String, Vec<String>>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigSecrets {
    pub provider: ConfigSecretsProviderToken,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigDesktop {
    pub notifications: ConfigDesktopNotifications,
    pub attention: ConfigDesktopAttention,
    pub system_tray: ConfigDesktopSystemTray,
    pub window_focus_blur: ConfigDesktopWindowFocusBlur,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigDesktopNotifications {
    pub enabled: Option<bool>,
    pub attention_on_incoming: Option<bool>,
    pub message_content_policy: ConfigNotificationsMessageContentPolicyToken,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigDesktopAttention {
    pub window_title: ConfigDesktopAttentionToggle,
    pub app_badge: ConfigDesktopAttentionToggle,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigDesktopAttentionToggle {
    pub enabled: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigDesktopSystemTray {
    pub enabled: Option<bool>,
    pub autostart: Option<bool>,
    pub icon_style: ConfigDesktopSystemTrayIconStyleToken,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigDesktopWindowFocusBlur {
    pub enabled: Option<bool>,
    pub delay_seconds: Option<i32>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigNetworkEncryption {
    pub only_verified_users: Option<bool>,
    pub share_with_trusted: Option<bool>,
    pub key_backup: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigCalls {
    pub legacy: ConfigCallsLegacy,
    pub element: ConfigCallsElement,
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
pub struct ConfigCallsElement {
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

#[derive(Clone, Debug, Default)]
pub struct ConfigNetwork {
    pub encryption: ConfigNetworkEncryption,
    pub presence_status_policy: ConfigNetworkPresenceStatusPolicyToken,
    pub tls_enable_certificate_validation: Option<bool>,
    pub mrs_enabled: Option<bool>,
    pub mrs_server_name: String,
    pub http3_enabled: Option<bool>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigIntegrations {
    pub dbus_api_access: ConfigIntegrationsDbusApiAccessToken,
    pub browser_command: String,
    pub transcription: ConfigIntegrationsTranscription,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigIntegrationsTranscription {
    pub provider: Option<ConfigIntegrationsTranscriptionProviderToken>,
    pub api_url: Option<String>,
    /// Populated from the secrets backend at load time, never from `config.yml`.
    /// See `var/plans/composer-voice-transcription.md` § "API key storage".
    pub api_key: Option<String>,
    pub model: Option<String>,
    pub language: Option<String>,
    pub prompt: Option<String>,
    pub by_room: BTreeMap<String, ConfigIntegrationsTranscriptionOverrides>,
}

/// Per-room override of transcription settings. Every field is optional;
/// unspecified fields fall back to the global value (and ultimately to the
/// built-in default). `api_key` is intentionally absent — per-room keys live
/// in the secrets backend keyed by a hash of the room id.
#[derive(Clone, Debug, Default)]
pub struct ConfigIntegrationsTranscriptionOverrides {
    pub provider: Option<ConfigIntegrationsTranscriptionProviderToken>,
    pub api_url: Option<String>,
    pub model: Option<String>,
    pub language: Option<String>,
    pub prompt: Option<String>,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigComposer {
    pub input_markdown_to_html_enabled: Option<bool>,
    pub input_send_key: ConfigComposerInputSendKeyToken,
    pub input_auto_replace_emoji: ConfigComposerInputAutoReplaceEmojiToken,
    pub input_emoji_preferred_gender: ConfigComposerEmojiPreferredGenderToken,
    pub input_emoji_preferred_skin_tone: ConfigComposerEmojiPreferredSkinToneToken,
    pub input_inline_emoji_picker_enabled: Option<bool>,
    pub input_inline_room_picker_enabled: Option<bool>,
    pub input_inline_user_picker_enabled: Option<bool>,
    pub input_selection_formatting_toolbar_enabled: Option<bool>,
    /// Master toggle for the composer voice-transcription gesture
    /// (long-press Space). The actual transcription provider config lives
    /// under `integrations.transcription.*`. See
    /// `var/plans/composer-voice-transcription.md` § "Config shape".
    pub input_transcription_enabled: Option<bool>,
    pub input_spellcheck_enabled: Option<bool>,
    pub input_spellcheck_languages: Option<Vec<String>>,
    pub attachments_strip_image_metadata: Option<bool>,
    pub typing_send: ConfigComposerTypingSend,
}

#[derive(Clone, Debug, Default)]
pub struct ConfigComposerTypingSend {
    pub global: Option<bool>,
    pub by_room: BTreeMap<String, bool>,
}

pub struct LoadedConfig {
    pub config: Config,
    pub source_exists: bool,
    pub source_version: i32,
    pub migrated_version: i32,
    pub had_future_version: bool,
    pub had_unsupported_path: bool,
    pub should_write_back: bool,
    pub serialized_yaml: String,
}
