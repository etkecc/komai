// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace settings::core {

enum class SettingScope
{
    Runtime,
    Config,
    State,
    Session,
    Secrets,
};

enum class SettingId
{
    Unknown,
    UiThemeSlug,
    UiScaleFactor,
    UiFontFamily,
    UiFontSizePt,
    UiFontEmojiFamily,
    UiMotionAnimationsEnabled,
    UiInputMode,
    UiInputTouchSwipeGesturesEnabled,
    UiAvatarsCircular,
    UiAvatarsDefaultAvatarStyle,
    UiLayoutDensity,
    NavigationRoomListShowLastMessageTime,
    NavigationRoomListLastMessagePreview,
    NavigationRoomListShowUnreadCounts,
    NavigationCommunitiesShowUnreadCounts,
    UiScrollbarPolicy,
    NavigationRoomListSort,
    NavigationCommunitiesFilterFavourites,
    NavigationCommunitiesFilterPeople,
    NavigationCommunitiesFilterBots,
    NavigationCommunitiesFilterGroups,
    NavigationCommunitiesFilterServerNotices,
    NavigationCommunitiesFilterLowPriority,
    NavigationRoomListOpeningPolicy,
    NavigationTabsShowPinButton,
    NavigationTabsPinnedTabLabel,
    NavigationTabsTabLabel,
    NavigationTabsPreferredWidthPx,
    NavigationTabsMinimumWidthPx,
    NavigationTabsMaxRecentlyClosedTimelines,
    DesktopSystemTrayEnabled,
    DesktopSystemTrayAutostart,
    DesktopNotificationsEnabled,
    DesktopNotificationsAttentionOnIncoming,
    DesktopNotificationsMessageContentPolicy,
    DesktopAttentionWindowTitleEnabled,
    DesktopAttentionAppBadgeEnabled,
    DesktopWindowFocusBlurEnabled,
    DesktopWindowFocusBlurDelaySeconds,
    NetworkPresenceStatusPolicy,
    IntegrationsDbusApiAccess,
    IntegrationsBrowserCommand,
    ComposerInputMarkdownToHtmlEnabled,
    ComposerInputSendKey,
    ComposerInputAutoReplaceEmoji,
    ComposerInputEmojiPreferredGender,
    ComposerInputEmojiPreferredSkinTone,
    ComposerInputInlineEmojiPickerEnabled,
    ComposerInputInlineRoomPickerEnabled,
    ComposerInputInlineUserPickerEnabled,
    ComposerTypingSendEnabled,
    ComposerExtrasStickersEnabled,
    NotificationsAccountEnabled,
    CallsLegacyEnabled,
    CallsRelayUseFallbackServer,
    CallsDevicesMicrophone,
    CallsDevicesCamera,
    CallsDevicesCameraResolution,
    CallsDevicesCameraFrameRate,
    CallsAudioRingtone,
    CallsScreenshareFrameRate,
    CallsScreensharePictureInPicture,
    CallsScreenshareIncludeRemoteVideo,
    CallsScreenshareShowCursor,
    NetworkTlsEnableCertificateValidation,
    NetworkHttp3Enabled,
    NetworkMrsEnabled,
    NetworkMrsServerName,
    TimelineMessagesStyle,
    TimelineMessagesLayoutPositioning,
    TimelineUserColorCodingPolicy,
    TimelineMessagesLayoutAvatarSize,
    TimelineMessagesLayoutShowOwnAvatar,
    TimelineMessagesLayoutMaxWidthPercent,
    TimelineMessagesSenderUsername,
    TimelineMessagesEmojiOnlyEnlarge,
    TimelineMessagesHoverHighlight,
    TimelineFormattedCodeSyntaxHighlighting,
    TimelineTypingShowEnabled,
    TimelineReadReceiptsEnabled,
    TimelineMessageActionsActivationPolicy,
    TimelineMessageActionsPinnedReactions,
    TimelineMediaEffectsEnabled,
    TimelineMediaAnimateOnHover,
    TimelineMediaImageDisplay,
    TimelineMediaOpenImagesExternal,
    TimelineMediaOpenVideosExternal,
    TimelineMediaAutoplayGifVideos,
    TimelineMediaOpenAudioExternal,
    TimelineMediaDefaultAudioPlaybackSpeed,
    TimelineThreadsCollapseRepliesGlobal,
    EncryptionKeySharingOnlyVerifiedUsers,
    EncryptionKeySharingShareWithTrusted,
    EncryptionBackupOnlineEnabled,
};

struct SettingDefinition
{
    SettingId id               = SettingId::Unknown;
    SettingScope scope         = SettingScope::Runtime;
    const char *persistedKey   = nullptr;
    bool requiresRestart       = false;
    bool hasIntRangeConstraint = false;
    int intRangeConstraintMin  = 0;
    int intRangeConstraintMax  = 0;
};

} // namespace settings::core
