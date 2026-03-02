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
    UiLayoutContentMaxWidthPx,
    UiAvatarsCircular,
    UiAvatarsIdenticonFallback,
    UiLayoutCompactMode,
    SidebarsRoomListShowLastMessageTime,
    SidebarsRoomListLastMessagePreview,
    SidebarsRoomListShowCommunityCounts,
    SidebarsRoomListScrollbarsEnabled,
    SidebarsRoomListSort,
    SidebarsCommunitiesVisible,
    NetworkPresenceStatusPolicy,
    PrivacyMaintenanceExpireEvents,
    PrivacyMaintenanceUpdateSpaceVias,
    PrivacyWindowFocusBlurEnabled,
    PrivacyWindowFocusBlurDelaySeconds,
    IntegrationsSystemTrayEnabled,
    IntegrationsSystemTrayAutostart,
    IntegrationsDbusApiAccess,
    IntegrationsBrowserCommand,
    ComposerInputMarkdownToHtmlEnabled,
    ComposerInputSendKey,
    ComposerInputAutoReplaceEmoji,
    ComposerTypingSendEnabled,
    ComposerExtrasStickersEnabled,
    NotificationsEnabled,
    NotificationsAttentionOnIncoming,
    NotificationsMessageContentPolicy,
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
    TimelineMessagesStyle,
    TimelineMessagesPositioning,
    TimelineUserColorCodingPolicy,
    TimelineMessagesLayoutSmallAvatars,
    TimelineMessagesLayoutShowOwnAvatar,
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
    EncryptionKeySharingOnlyVerifiedUsers,
    EncryptionKeySharingShareWithTrusted,
    EncryptionBackupOnlineEnabled,
    EncryptionOnlineBackupKeyStatus,
    EncryptionSelfSigningKeyStatus,
    EncryptionUserSigningKeyStatus,
    EncryptionMasterSigningKeyStatus,
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
