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
constexpr auto SchemaVersion                    = "meta.settings_schema_version";
constexpr auto ConfigSchemaVersion              = SchemaVersion;
constexpr auto DesktopSystemTrayEnabled         = "desktop.system_tray.enabled";
constexpr auto DesktopSystemTrayAutostart       = "desktop.system_tray.autostart";
constexpr auto UiThemeSlug                      = "ui.theme.slug";
constexpr auto UiFontFamily                     = "ui.font.family";
constexpr auto UiFontEmojiFamily                = "ui.font.emoji_family";
constexpr auto UiFontSizePt                     = "ui.font.size_pt";
constexpr auto UiScaleFactor                    = "ui.scale.factor";
constexpr auto UiMotionAnimationsEnabled        = "ui.motion.enable_animations";
constexpr auto UiInputMode                      = "ui.input.mode";
constexpr auto UiInputTouchSwipeGesturesEnabled = "ui.input.touch.swipe_gestures.enabled";
constexpr auto UiLayoutContentMaxWidthPx        = "ui.layout.content.max_width_px";
constexpr auto UiAvatarsCircular                = "ui.avatars.circular";
constexpr auto UiAvatarsDefaultAvatarStyle      = "ui.avatars.default_avatar_style";
constexpr auto UiLayoutCompactMode              = "ui.layout.compact_mode";
constexpr auto SidebarsRoomListShowLastMessageTime =
  "sidebars.room_list.show_last_message_timestamp";
constexpr auto SidebarsRoomListLastMessagePreview = "sidebars.room_list.last_message_preview";
constexpr auto SidebarsRoomListShowCommunityCounts =
  "sidebars.room_list.show_community_notification_counts";
constexpr auto UiScrollbarPolicy                     = "ui.scrollbar_policy";
constexpr auto SidebarsRoomListSort                  = "sidebars.room_list.sort";
constexpr auto SidebarsRoomListUnreadDetectionPolicy = "sidebars.room_list.unread_detection_policy";
constexpr auto SidebarsCommunitiesVisible            = "sidebars.communities.visible";
constexpr auto SidebarsCommunitiesFilterFavourites   = "sidebars.communities.filters.favourites";
constexpr auto SidebarsCommunitiesFilterPeople       = "sidebars.communities.filters.people";
constexpr auto SidebarsCommunitiesFilterBots         = "sidebars.communities.filters.bots";
constexpr auto SidebarsCommunitiesFilterGroups       = "sidebars.communities.filters.groups";
constexpr auto SidebarsCommunitiesFilterServerNotices =
  "sidebars.communities.filters.server_notices";
constexpr auto SidebarsCommunitiesFilterLowPriority = "sidebars.communities.filters.low_priority";
constexpr auto TimelineMessagesStyle                = "timeline.messages.style";
constexpr auto TimelineMessagesPositioning          = "timeline.messages.positioning";
constexpr auto TimelineUserColorCodingPolicy        = "timeline.user_color_coding_policy";
constexpr auto TimelineMessagesLayoutSmallAvatars   = "timeline.messages.layout.small_avatars";
constexpr auto TimelineMessagesLayoutShowOwnAvatar  = "timeline.messages.layout.show_own_avatar";
constexpr auto TimelineMessagesSenderUsername       = "timeline.messages.sender_username";
constexpr auto TimelineMessagesEmojiOnlyEnlarge     = "timeline.messages.emoji_only_enlarge";
constexpr auto TimelineMessagesHoverHighlight       = "timeline.messages.hover_highlight";
constexpr auto TimelineFormattedCodeSyntaxHighlighting =
  "timeline.messages.formatted.code_syntax_highlighting";
constexpr auto TimelineTypingShowEnabled   = "timeline.typing.show.enabled";
constexpr auto TimelineReadReceiptsEnabled = "timeline.read_receipts.enabled";
constexpr auto TimelineMessageActionsActivationPolicy =
  "timeline.messages.actions.activation_policy";
constexpr auto TimelineMessageActionsPinnedReactions = "timeline.messages.actions.pinned_reactions";
constexpr auto TimelineMediaEffectsEnabled           = "timeline.media.effects.enabled";
constexpr auto TimelineMediaAnimateOnHover           = "timeline.media.animate_on_hover";
constexpr auto TimelineMediaImageDisplay             = "timeline.media.image_display";
constexpr auto TimelineMediaOpenImagesExternal       = "timeline.media.open_images_external";
constexpr auto TimelineMediaOpenVideosExternal       = "timeline.media.open_videos_external";
constexpr auto TimelineMediaAutoplayGifVideos        = "timeline.media.autoplay_gif_videos";
constexpr auto TimelineMediaOpenAudioExternal        = "timeline.media.open_audio_external";
constexpr auto TimelineMediaDefaultAudioPlaybackSpeed =
  "timeline.media.default_audio_playback_speed";
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
constexpr auto ComposerTypingSendEnabled             = "composer.typing.send.enabled";
constexpr auto ComposerExtrasStickersEnabled         = "composer.extras.stickers.enabled";
constexpr auto DesktopNotificationsEnabled           = "desktop.notifications.enabled";
constexpr auto DesktopNotificationsAttentionOnIncoming =
  "desktop.notifications.attention_on_incoming";
constexpr auto DesktopNotificationsMessageContentPolicy =
  "desktop.notifications.message_content_policy";
constexpr auto DesktopAttentionWindowTitleEnabled    = "desktop.attention.window_title.enabled";
constexpr auto DesktopAttentionAppBadgeEnabled       = "desktop.attention.app_badge.enabled";
constexpr auto CallsLegacyEnabled                    = "calls.legacy.enabled";
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
constexpr auto SecretsProvider                       = "secrets.provider";

// state.yml
constexpr auto StateSchemaVersion            = SchemaVersion;
constexpr auto UiWindowWidthPx               = "ui.window.width_px";
constexpr auto UiWindowHeightPx              = "ui.window.height_px";
constexpr auto SidebarsRoomListWidthPx       = "sidebars.room_list.width_px";
constexpr auto SidebarsRoomListCurrentRoomId = "sidebars.room_list.current_room_id";
constexpr auto SidebarsCommunitiesWidthPx    = "sidebars.communities.width_px";
constexpr auto SidebarsCommunitiesFilteringGlobalExcludes =
  "sidebars.communities.filtering.global_excludes";
constexpr auto SidebarsCommunitiesFilteringBadgesHidden =
  "sidebars.communities.filtering.badges_hidden";
constexpr auto SidebarsCommunitiesFilteringCollapsedSpaces =
  "sidebars.communities.filtering.collapsed_spaces";
constexpr auto SidebarsCommunitiesFilteringCurrent = "sidebars.communities.filtering.current";
constexpr auto TimelinePinsHidden                  = "timeline.pins.hidden";
constexpr auto TimelineWidgetsHidden               = "timeline.widgets.hidden";
constexpr auto ComposerDraftsByRoom                = "composer.drafts.by_room";

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
