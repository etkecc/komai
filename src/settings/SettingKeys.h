// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace SettingKey {
/**
 * Canonical dotted keys used by settings load/save and migration-safe reads.
 */
// config.yml
constexpr auto SchemaVersion               = "meta.settings_schema_version";
constexpr auto ConfigSchemaVersion         = SchemaVersion;
constexpr auto DesktopSystemTrayEnabled    = "desktop.system_tray.enabled";
constexpr auto DesktopSystemTrayAutostart  = "desktop.system_tray.autostart";
constexpr auto DesktopSystemTrayIconStyle  = "desktop.system_tray.icon_style";
constexpr auto UiThemeSlug                 = "ui.theme.slug";
constexpr auto UiThemeMode                 = "ui.theme.mode";
constexpr auto UiFontFamily                = "ui.font.family";
constexpr auto UiFontEmojiFamily           = "ui.font.emoji_family";
constexpr auto UiFontSizePt                = "ui.font.size_pt";
constexpr auto UiScaleFactor               = "ui.scale.factor";
constexpr auto UiMotionAnimationsEnabled   = "ui.motion.enable_animations";
constexpr auto UiAvatarsCircular           = "ui.avatars.circular";
constexpr auto UiAvatarsDefaultAvatarStyle = "ui.avatars.default_avatar_style";
constexpr auto UiLayoutDensity             = "ui.layout.density";
constexpr auto UiLanguage                  = "ui.language";
constexpr auto NavigationRoomListShowLastMessageTime =
  "navigation.room_list.show_last_message_timestamp";
constexpr auto NavigationRoomListLastMessagePreview = "navigation.room_list.last_message_preview";
constexpr auto NavigationRoomListShowUnreadIndicators =
  "navigation.room_list.show_unread_indicators";
constexpr auto NavigationCommunitiesShowUnreadIndicators =
  "navigation.communities.show_unread_indicators";
constexpr auto UiScrollbarPolicy                     = "ui.scrollbar_policy";
constexpr auto NavigationRoomListSort                = "navigation.room_list.sort";
constexpr auto NavigationRoomListOpeningPolicy       = "navigation.room_list.opening_policy";
constexpr auto NavigationCommunitiesFilterFavourites = "navigation.communities.filters.favourites";
constexpr auto NavigationCommunitiesFilterPeople     = "navigation.communities.filters.people";
constexpr auto NavigationCommunitiesFilterBots       = "navigation.communities.filters.bots";
constexpr auto NavigationCommunitiesFilterGroups     = "navigation.communities.filters.groups";
constexpr auto NavigationCommunitiesFilterServerNotices =
  "navigation.communities.filters.server_notices";
constexpr auto NavigationCommunitiesFilterLowPriority =
  "navigation.communities.filters.low_priority";
constexpr auto NavigationTabsAutoHideSingle   = "navigation.tabs.auto_hide_with_single_tab";
constexpr auto NavigationTabsShowPinButton    = "navigation.tabs.show_pin_button";
constexpr auto NavigationTabsPinnedTabLabel   = "navigation.tabs.pinned_tab_label";
constexpr auto NavigationTabsTabLabel         = "navigation.tabs.tab_label";
constexpr auto NavigationTabsPreferredWidthPx = "navigation.tabs.preferred_width_px";
constexpr auto NavigationTabsMinimumWidthPx   = "navigation.tabs.minimum_width_px";
constexpr auto NavigationTabsMaxRecentlyClosedTimelines =
  "navigation.tabs.max_recently_closed_timelines";
constexpr auto TimelineMessagesStyle                 = "timeline.messages.style";
constexpr auto TimelineMessagesLayoutPositioning     = "timeline.messages.layout.positioning";
constexpr auto TimelineUserColorCodingPolicy         = "timeline.user_color_coding_policy";
constexpr auto TimelineMessagesLayoutAvatarSize      = "timeline.messages.layout.avatar_size";
constexpr auto TimelineMessagesLayoutShowOwnAvatar   = "timeline.messages.layout.show_own_avatar";
constexpr auto TimelineMessagesLayoutMaxWidthPercent = "timeline.messages.layout.max_width_percent";
constexpr auto TimelineMessagesLayoutAdaptivePositioningBreakpointPx =
  "timeline.messages.layout.adaptive_positioning_breakpoint_px";
constexpr auto TimelineMessagesSenderUsername   = "timeline.messages.sender_username";
constexpr auto TimelineMessagesEmojiOnlyEnlarge = "timeline.messages.emoji_only_enlarge";
constexpr auto TimelineMessagesHoverHighlight   = "timeline.messages.hover_highlight";
constexpr auto TimelineMessagesDragSelect       = "timeline.messages.drag_select";
constexpr auto TimelineFormattedCodeSyntaxHighlighting =
  "timeline.messages.formatted.code_syntax_highlighting";
constexpr auto TimelineTypingShowEnabled  = "timeline.typing.show.enabled";
constexpr auto TimelineReadReceiptsGlobal = "timeline.read_receipts.global";
constexpr auto TimelineReadReceiptsByRoom = "timeline.read_receipts.by_room";
constexpr auto TimelineMessageActionsActivationPolicy =
  "timeline.messages.actions.activation_policy";
constexpr auto TimelineMessageActionsPinnedReactions = "timeline.messages.actions.pinned_reactions";
constexpr auto TimelineMediaEffectsEnabled           = "timeline.media.effects.enabled";
constexpr auto TimelineDateDividersEnabled           = "timeline.date_dividers.enabled";
constexpr auto TimelineRoomHeaderButtonLabels        = "timeline.room_header.button_labels";
constexpr auto TimelineMediaAnimateOnHover           = "timeline.media.animate_on_hover";
constexpr auto TimelineMediaImageDisplay             = "timeline.media.image_display";
constexpr auto TimelineMediaOpenImagesExternal       = "timeline.media.open_images_external";
constexpr auto TimelineMediaOpenVideosExternal       = "timeline.media.open_videos_external";
constexpr auto TimelineMediaAutoplayGifVideos        = "timeline.media.autoplay_gif_videos";
constexpr auto TimelineMediaOpenAudioExternal        = "timeline.media.open_audio_external";
constexpr auto TimelineMediaDefaultAudioPlaybackSpeed =
  "timeline.media.default_audio_playback_speed";
constexpr auto TimelineThreadsCollapseRepliesGlobal  = "timeline.threads.collapse_replies.global";
constexpr auto TimelineThreadsCollapseRepliesByRoom  = "timeline.threads.collapse_replies.by_room";
constexpr auto TimelineHiddenEventsGlobal            = "timeline.hidden_events.global";
constexpr auto TimelineHiddenEventsByRoom            = "timeline.hidden_events.by_room";
constexpr auto ComposerInputMarkdownToHtmlEnabled    = "composer.input.markdown_to_html.enabled";
constexpr auto ComposerInputSendKey                  = "composer.input.send_key";
constexpr auto ComposerInputAutoReplaceEmoji         = "composer.input.auto_replace_emoji";
constexpr auto ComposerInputEmojiPreferredGender     = "composer.input.emoji.preferred_gender";
constexpr auto ComposerInputEmojiPreferredSkinTone   = "composer.input.emoji.preferred_skin_tone";
constexpr auto ComposerInputInlineEmojiPickerEnabled = "composer.input.inline_emoji_picker.enabled";
constexpr auto ComposerInputInlineRoomPickerEnabled  = "composer.input.inline_room_picker.enabled";
constexpr auto ComposerInputInlineUserPickerEnabled  = "composer.input.inline_user_picker.enabled";
constexpr auto ComposerInputSelectionFormattingToolbarEnabled =
  "composer.input.selection_formatting_toolbar.enabled";
constexpr auto ComposerInputTranscriptionEnabled     = "composer.input.transcription.enabled";
constexpr auto ComposerInputSpellcheckEnabled        = "composer.input.spellcheck.enabled";
constexpr auto ComposerInputSpellcheckLanguages      = "composer.input.spellcheck.languages";
constexpr auto ComposerAttachmentsStripImageMetadata = "composer.attachments.strip_image_metadata";
constexpr auto ComposerTypingSendGlobal              = "composer.typing.send.global";
constexpr auto ComposerTypingSendByRoom              = "composer.typing.send.by_room";
constexpr auto DesktopNotificationsEnabled           = "desktop.notifications.enabled";
constexpr auto DesktopNotificationsAttentionOnIncoming =
  "desktop.notifications.attention_on_incoming";
constexpr auto DesktopNotificationsMessageContentPolicy =
  "desktop.notifications.message_content_policy";
constexpr auto DesktopAttentionWindowTitleEnabled    = "desktop.attention.window_title.enabled";
constexpr auto DesktopAttentionAppBadgeEnabled       = "desktop.attention.app_badge.enabled";
constexpr auto CallsLegacyEnabled                    = "calls.legacy.enabled";
constexpr auto CallsElementEnabled                   = "calls.element.enabled";
constexpr auto CallsRelayUseFallbackServer           = "calls.relay.use_fallback_server";
constexpr auto CallsDevicesMicrophone                = "calls.devices.microphone";
constexpr auto CallsDevicesCamera                    = "calls.devices.camera";
constexpr auto CallsDevicesCameraResolution          = "calls.devices.camera_resolution";
constexpr auto CallsDevicesCameraFrameRate           = "calls.devices.camera_frame_rate";
constexpr auto CallsAudioRingtone                    = "calls.audio.ringtone";
constexpr auto CallsScreenshareFrameRate             = "calls.screenshare.frame_rate";
constexpr auto CallsScreensharePictureInPicture      = "calls.screenshare.picture_in_picture";
constexpr auto CallsScreenshareIncludeRemoteVideo    = "calls.screenshare.include_remote_video";
constexpr auto CallsScreenshareShowCursor            = "calls.screenshare.show_cursor";
constexpr auto DesktopWindowFocusBlurEnabled         = "desktop.window_focus_blur.enabled";
constexpr auto DesktopWindowFocusBlurDelaySeconds    = "desktop.window_focus_blur.delay_seconds";
constexpr auto EncryptionKeySharingOnlyVerifiedUsers = "network.encryption.only_verified_users";
constexpr auto EncryptionKeySharingShareWithTrusted  = "network.encryption.share_with_trusted";
constexpr auto EncryptionBackupOnlineEnabled         = "network.encryption.key_backup";
constexpr auto NetworkTlsEnableCertificateValidation = "network.tls.enable_certificate_validation";
constexpr auto NetworkHttp3Enabled                   = "network.http3.enabled";
constexpr auto NetworkMrsEnabled                     = "network.mrs.enabled";
constexpr auto NetworkMrsServerName                  = "network.mrs.server_name";
constexpr auto NetworkPresenceStatusPolicy           = "network.presence.status_policy";
constexpr auto IntegrationsDbusApiAccess             = "integrations.dbus.access";
constexpr auto IntegrationsBrowserCommand            = "integrations.browser.command";
constexpr auto IntegrationsTranscriptionProvider     = "integrations.transcription.provider";
constexpr auto IntegrationsTranscriptionApiUrl       = "integrations.transcription.api_url";
constexpr auto IntegrationsTranscriptionModel        = "integrations.transcription.model";
constexpr auto IntegrationsTranscriptionLanguage     = "integrations.transcription.language";
constexpr auto IntegrationsTranscriptionPrompt       = "integrations.transcription.prompt";
constexpr auto SecretsProvider                       = "secrets.provider";

// state.yml
constexpr auto StateSchemaVersion              = SchemaVersion;
constexpr auto UiWindowWidthPx                 = "ui.window.width_px";
constexpr auto UiWindowHeightPx                = "ui.window.height_px";
constexpr auto NavigationRoomListWidthPx       = "navigation.room_list.width_px";
constexpr auto NavigationRoomListCurrentRoomId = "navigation.room_list.current_room_id";
constexpr auto NavigationCommunitiesWidthPx    = "navigation.communities.width_px";
constexpr auto NavigationCommunitiesFilteringGlobalExcludes =
  "navigation.communities.filtering.global_excludes";
constexpr auto NavigationCommunitiesFilteringUnreadIndicatorsHidden =
  "navigation.communities.filtering.unread_indicators_hidden";
constexpr auto NavigationCommunitiesFilteringCollapsedSpaces =
  "navigation.communities.filtering.collapsed_spaces";
constexpr auto NavigationCommunitiesFilteringCurrent = "navigation.communities.filtering.current";
constexpr auto TimelinePinsHidden                    = "timeline.pins.hidden";
constexpr auto TimelineWidgetsHidden                 = "timeline.widgets.hidden";
constexpr auto ComposerDraftsByRoom                  = "composer.drafts.by_room";
constexpr auto DesktopSystemTrayFirstClosePrompted   = "desktop.system_tray.first_close_prompted";

// session.yml
constexpr auto SessionSchemaVersion     = SchemaVersion;
constexpr auto SessionAccountUserId     = "session.account.user_id";
constexpr auto SessionAccountHomeserver = "session.account.homeserver";
constexpr auto SessionDeviceId          = "session.device.id";

// secrets.yml (file provider fallback only)
constexpr auto SecretsFileMap = "secrets";
} // namespace SettingKey

constexpr int IntegrationsDbusAccessNone      = 0;
constexpr int IntegrationsDbusAccessReadOnly  = 1;
constexpr int IntegrationsDbusAccessReadWrite = 2;

constexpr auto SecureStoreSecretsKey = "session.secrets";
