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
constexpr auto IntegrationsSystemTrayEnabled    = "integrations.system_tray.enabled";
constexpr auto IntegrationsSystemTrayAutostart  = "integrations.system_tray.autostart";
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
constexpr auto UiAvatarsIdenticonFallback       = "ui.avatars.identicon_fallback";
constexpr auto UiLayoutCompactMode              = "ui.layout.compact_mode";
constexpr auto SidebarsRoomListShowLastMessageTime =
  "sidebars.room_list.show_last_message_timestamp";
constexpr auto SidebarsRoomListLastMessagePreview = "sidebars.room_list.last_message_preview";
constexpr auto SidebarsRoomListShowCommunityCounts =
  "sidebars.room_list.show_community_notification_counts";
constexpr auto SidebarsRoomListScrollbarsEnabled   = "sidebars.room_list.scrollbars.visible";
constexpr auto SidebarsRoomListSort                = "sidebars.room_list.sort";
constexpr auto SidebarsCommunitiesVisible          = "sidebars.communities.visible";
constexpr auto TimelineMessagesLayoutStyle         = "timeline.messages.layout.style";
constexpr auto TimelineMessagesLayoutSmallAvatars  = "timeline.messages.layout.small_avatars";
constexpr auto TimelineMessagesLayoutShowOwnAvatar = "timeline.messages.layout.show_own_avatar";
constexpr auto TimelineMessagesSenderUsername      = "timeline.messages.sender_username";
constexpr auto TimelineMessagesMaxWidthPx          = "timeline.messages.max_width_px";
constexpr auto TimelineMessagesEmojiOnlyEnlarge    = "timeline.messages.emoji_only_enlarge";
constexpr auto TimelineMessagesHoverHighlight      = "timeline.messages.hover_highlight";
constexpr auto TimelineTypingShowEnabled           = "timeline.typing.show.enabled";
constexpr auto TimelineReadReceiptsEnabled         = "timeline.read_receipts.enabled";
constexpr auto TimelineMessageActionsActivationPolicy =
  "timeline.messages.actions.activation_policy";
constexpr auto TimelineMessageActionsPinnedReactions = "timeline.messages.actions.pinned_reactions";
constexpr auto TimelineMediaEffectsEnabled           = "timeline.media.effects.enabled";
constexpr auto TimelineMediaAnimateOnHover           = "timeline.media.animate_on_hover";
constexpr auto TimelineMediaImageDisplay             = "timeline.media.image_display";
constexpr auto TimelineMediaOpenImagesExternal       = "timeline.media.open_images_external";
constexpr auto TimelineMediaOpenVideosExternal       = "timeline.media.open_videos_external";
constexpr auto ComposerInputMarkdownEnabled          = "composer.input.markdown.enabled";
constexpr auto ComposerInputSendKey                  = "composer.input.send_key";
constexpr auto ComposerInputAutoReplaceEmoji         = "composer.input.auto_replace_emoji";
constexpr auto ComposerTypingSendEnabled             = "composer.typing.send.enabled";
constexpr auto ComposerExtrasStickersEnabled         = "composer.extras.stickers.enabled";
constexpr auto NotificationsEnabled                  = "notifications.enabled";
constexpr auto NotificationsAttentionOnIncoming      = "notifications.attention_on_incoming";
constexpr auto NotificationsMessageContentPolicy     = "notifications.message_content_policy";
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
constexpr auto PrivacyWindowFocusBlurEnabled         = "privacy.window_focus_blur.enabled";
constexpr auto PrivacyWindowFocusBlurDelaySeconds    = "privacy.window_focus_blur.delay_seconds";
constexpr auto PrivacyMaintenanceExpireEvents        = "privacy.maintenance.expire_events";
constexpr auto PrivacyMaintenanceUpdateSpaceVias     = "privacy.maintenance.update_space_vias";
constexpr auto EncryptionKeySharingOnlyVerifiedUsers = "encryption.key_sharing.only_verified_users";
constexpr auto EncryptionKeySharingShareWithTrusted  = "encryption.key_sharing.share_with_trusted";
constexpr auto EncryptionBackupOnlineEnabled         = "encryption.backup.online.enabled";
constexpr auto NetworkTlsEnableCertificateValidation = "network.tls.enable_certificate_validation";
constexpr auto NetworkHttp3Enabled                   = "network.http3.enabled";
constexpr auto NetworkPresenceStatusPolicy           = "network.presence.status_policy";
constexpr auto DbMaxSizeBytes                        = "db.max_size_bytes";
constexpr auto DbMaxStores                           = "db.max_stores";
constexpr auto IntegrationsDbusApiAccess             = "integrations.dbus.access";
constexpr auto IntegrationsBrowserCommand            = "integrations.browser.command";
constexpr auto SecretsProvider                       = "secrets.provider";

// state.yml
constexpr auto StateSchemaVersion                 = SchemaVersion;
constexpr auto AppWindowSizeWidth                 = "app.window.size.width";
constexpr auto AppWindowSizeHeight                = "app.window.size.height";
constexpr auto SidebarsRoomListWidthPx            = "sidebars.room_list.width_px";
constexpr auto SidebarsCommunitiesWidthPx         = "sidebars.communities.width_px";
constexpr auto SidebarsCommunitiesHiddenTags      = "sidebars.communities.hidden_tags";
constexpr auto SidebarsCommunitiesMutedTags       = "sidebars.communities.muted_tags";
constexpr auto SidebarsCommunitiesCollapsedSpaces = "sidebars.communities.collapsed_spaces";
constexpr auto SessionNavigationCurrentTagId      = "session.navigation.current_tag_id";
constexpr auto TimelinePinsHidden                 = "timeline.pins.hidden";
constexpr auto TimelineWidgetsHidden              = "timeline.widgets.hidden";
constexpr auto ComposerReactionsRecent            = "composer.reactions.recent";

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
