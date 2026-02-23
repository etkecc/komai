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
    NetworkPresenceStatusPolicy,
    PrivacyScreenLockEnabled,
    PrivacyScreenLockTimeoutSeconds,
    IntegrationsSystemTrayEnabled,
    IntegrationsSystemTrayAutostart,
    IntegrationsDbusApiAccess,
    ComposerInputMarkdownEnabled,
    ComposerInputSendKey,
    ComposerInputAutoReplaceEmoji,
    ComposerFeedbackTypingNotifications,
    ComposerFeedbackReadReceipts,
    ComposerExtrasStickersEnabled,
    NotificationsDesktopEnabled,
    NotificationsDesktopAlertOnIncoming,
    NotificationsDesktopDecryptMessages,
    CallsLegacyEnabled,
    CallsRelayUseFallbackServer,
    CallsDevicesMicrophone,
    CallsDevicesCamera,
    CallsDevicesCameraResolution,
    CallsDevicesCameraFrameRate,
    CallsAudioRingtone,
    TimelineMessagesLayoutBubbles,
    TimelineMessagesLayoutSmallAvatars,
    TimelineMessagesLayoutShowOwnAvatar,
    TimelineMessagesSenderUsername,
    TimelineMessagesMaxWidthPx,
    TimelineMessagesEmojiOnlyEnlarge,
    TimelineMessagesHoverHighlight,
    TimelineMessageActionsEnabled,
    TimelineMessageActionsPinnedReactions,
    TimelineMediaEffectsEnabled,
    TimelineMediaAnimateOnHover,
    TimelineMediaImageDisplay,
    TimelineMediaOpenImagesExternal,
    TimelineMediaOpenVideosExternal,
    EncryptionOnlineBackupKeyStatus,
    EncryptionSelfSigningKeyStatus,
    EncryptionUserSigningKeyStatus,
    EncryptionMasterSigningKeyStatus,
};

struct SettingDefinition
{
    SettingId id             = SettingId::Unknown;
    SettingScope scope       = SettingScope::Runtime;
    const char *persistedKey = nullptr;
    bool requiresRestart     = false;
};

} // namespace settings::core
