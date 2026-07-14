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
    UiThemeMode,
    UiScaleFactor,
    UiFontFamily,
    UiFontSizePt,
    UiFontEmojiFamily,
    UiMotionAnimationsEnabled,
    UiAvatarsCircular,
    UiAvatarsDefaultAvatarStyle,
    UiLayoutDensity,
    UiLanguage,
    NavigationRoomListShowLastMessageTime,
    NavigationRoomListLastMessagePreview,
    NavigationRoomListShowUnreadIndicators,
    NavigationCommunitiesShowUnreadIndicators,
    UiScrollbarPolicy,
    NavigationRoomListSort,
    NavigationCommunitiesFilterFavourites,
    NavigationCommunitiesFilterPeople,
    NavigationCommunitiesFilterBots,
    NavigationCommunitiesFilterGroups,
    NavigationCommunitiesFilterServerNotices,
    NavigationCommunitiesFilterLowPriority,
    NavigationRoomListOpeningPolicy,
    NavigationTabsAutoHideSingle,
    NavigationTabsShowPinButton,
    NavigationTabsPinnedTabLabel,
    NavigationTabsTabLabel,
    NavigationTabsPreferredWidthPx,
    NavigationTabsMinimumWidthPx,
    NavigationTabsMaxRecentlyClosedTimelines,
    DesktopSystemTrayEnabled,
    DesktopSystemTrayAutostart,
    DesktopSystemTrayIconStyle,
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
    IntegrationsTranscriptionProvider,
    IntegrationsTranscriptionApiUrl,
    IntegrationsTranscriptionModel,
    IntegrationsTranscriptionLanguage,
    IntegrationsTranscriptionPrompt,
    ComposerInputMarkdownToHtmlEnabled,
    ComposerInputSendKey,
    ComposerInputAutoReplaceEmoji,
    ComposerInputEmojiPreferredGender,
    ComposerInputEmojiPreferredSkinTone,
    ComposerInputInlineEmojiPickerEnabled,
    ComposerInputInlineRoomPickerEnabled,
    ComposerInputInlineUserPickerEnabled,
    ComposerInputSelectionFormattingToolbarEnabled,
    ComposerInputTranscriptionEnabled,
    ComposerAttachmentsStripImageMetadata,
    ComposerTypingSendGlobal,
    NotificationsAccountEnabled,
    CallsLegacyEnabled,
    CallsElementEnabled,
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
    TimelineMessagesLayoutAdaptivePositioningBreakpointPx,
    TimelineMessagesSenderUsername,
    TimelineMessagesEmojiOnlyEnlarge,
    TimelineMessagesHoverHighlight,
    TimelineMessagesDragSelect,
    TimelineFormattedCodeSyntaxHighlighting,
    TimelineTypingShowEnabled,
    TimelineReadReceiptsGlobal,
    TimelineMessageActionsActivationPolicy,
    TimelineMessageActionsPinnedReactions,
    TimelineMediaEffectsEnabled,
    TimelineDateDividersEnabled,
    TimelineMediaAnimateOnHover,
    TimelineMediaImageDisplay,
    TimelineMediaOpenImagesExternal,
    TimelineMediaOpenVideosExternal,
    TimelineMediaAutoplayGifVideos,
    TimelineMediaOpenAudioExternal,
    TimelineMediaDefaultAudioPlaybackSpeed,
    TimelineThreadsCollapseRepliesGlobal,
    TimelineRoomHeaderButtonLabels,
    EncryptionKeySharingOnlyVerifiedUsers,
    EncryptionKeySharingShareWithTrusted,
    EncryptionBackupOnlineEnabled,
    ComposerInputSpellcheckEnabled,
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
