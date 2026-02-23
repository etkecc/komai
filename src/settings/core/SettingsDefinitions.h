// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <optional>
#include <span>

#include "settings/SettingKeys.h"
#include "settings/core/SettingDefinition.h"

namespace settings::core::definitions {

inline constexpr int kDefaultSidebarsRoomListWidthPx       = 300;
inline constexpr int kDefaultSidebarsCommunitiesWidthPx    = 200;
inline constexpr bool kDefaultUiMotionAnimationsEnabled    = true;
inline constexpr bool kDefaultInputEnableTextSelection     = true;
inline constexpr double kDefaultScaleFactor                = -1.0;
inline constexpr double kDefaultFontSizePt                 = 13.0;
inline constexpr int kDefaultScreenShareFrameRate          = 5;
inline constexpr int kDefaultPrivacyScreenTimeoutSeconds   = 0;
inline constexpr int kDefaultTimelineMaxWidthPx            = 0;
inline constexpr unsigned int kDefaultMaxDbs               = 0;
inline constexpr unsigned long long kDefaultMaxDbSizeBytes = 0;
inline constexpr int kDefaultIntegrationsDbusApiAccess     = 0;

inline constexpr std::array<SettingDefinition, 58> kPersistedSettingDefinitions{{
  {SettingId::UiThemeSlug, SettingScope::Config, SettingKey::UiThemeSlug, false},
  {SettingId::UiFontFamily, SettingScope::Config, SettingKey::UiFontFamily, false},
  {SettingId::UiFontSizePt, SettingScope::Config, SettingKey::UiFontSizePt, false},
  {SettingId::UiFontEmojiFamily, SettingScope::Config, SettingKey::UiFontEmojiFamily, false},
  {SettingId::UiMotionAnimationsEnabled,
   SettingScope::Config,
   SettingKey::UiMotionAnimationsEnabled,
   false},
  {SettingId::UiInputEnableTextSelection,
   SettingScope::Config,
   SettingKey::UiInputEnableTextSelection,
   false},
  {SettingId::UiInputSwipeGestures, SettingScope::Config, SettingKey::UiInputSwipeGestures, false},
  {SettingId::UiAvatarsCircular, SettingScope::Config, SettingKey::UiAvatarsCircular, false},
  {SettingId::UiAvatarsIdenticonFallback,
   SettingScope::Config,
   SettingKey::UiAvatarsIdenticonFallback,
   false},
  {SettingId::SidebarsRoomListCompact,
   SettingScope::Config,
   SettingKey::SidebarsRoomListCompact,
   false},
  {SettingId::SidebarsRoomListShowLastMessageTime,
   SettingScope::Config,
   SettingKey::SidebarsRoomListShowLastMessageTime,
   false},
  {SettingId::SidebarsRoomListLastMessagePreview,
   SettingScope::Config,
   SettingKey::SidebarsRoomListLastMessagePreview,
   false,
   true,
   0,
   2},
  {SettingId::SidebarsRoomListShowCommunityCounts,
   SettingScope::Config,
   SettingKey::SidebarsRoomListShowCommunityCounts,
   false},
  {SettingId::SidebarsRoomListScrollbarsEnabled,
   SettingScope::Config,
   SettingKey::SidebarsRoomListScrollbarsEnabled,
   false},
  {SettingId::SidebarsRoomListSort,
   SettingScope::Config,
   SettingKey::SidebarsRoomListSort,
   false,
   true,
   0,
   3},
  {SettingId::SidebarsCommunitiesVisible,
   SettingScope::Config,
   SettingKey::SidebarsCommunitiesVisible,
   false},
  {SettingId::NetworkPresenceStatusPolicy,
   SettingScope::Config,
   SettingKey::NetworkPresenceStatusPolicy,
   false,
   true,
   0,
   3},
  {SettingId::PrivacyMaintenanceExpireEvents,
   SettingScope::Config,
   SettingKey::PrivacyMaintenanceExpireEvents,
   false},
  {SettingId::PrivacyMaintenanceUpdateSpaceVias,
   SettingScope::Config,
   SettingKey::PrivacyMaintenanceUpdateSpaceVias,
   false},
  {SettingId::PrivacyScreenLockEnabled,
   SettingScope::Config,
   SettingKey::PrivacyScreenLockEnabled,
   false},
  {SettingId::PrivacyScreenLockTimeoutSeconds,
   SettingScope::Config,
   SettingKey::PrivacyScreenLockTimeoutSeconds,
   false,
   true,
   0,
   3600},
  {SettingId::IntegrationsSystemTrayEnabled,
   SettingScope::Config,
   SettingKey::IntegrationsSystemTrayEnabled,
   false},
  {SettingId::IntegrationsSystemTrayAutostart,
   SettingScope::Config,
   SettingKey::IntegrationsSystemTrayAutostart,
   false},
  {SettingId::IntegrationsDbusApiAccess,
   SettingScope::Config,
   SettingKey::IntegrationsDbusApiAccess,
   false,
   true,
   0,
   2},
  {SettingId::IntegrationsBrowserCommand,
   SettingScope::Config,
   SettingKey::IntegrationsBrowserCommand,
   false},
  {SettingId::ComposerInputMarkdownEnabled,
   SettingScope::Config,
   SettingKey::ComposerInputMarkdownEnabled,
   false},
  {SettingId::ComposerInputSendKey,
   SettingScope::Config,
   SettingKey::ComposerInputSendKey,
   false,
   true,
   0,
   2},
  {SettingId::ComposerInputAutoReplaceEmoji,
   SettingScope::Config,
   SettingKey::ComposerInputAutoReplaceEmoji,
   false,
   true,
   0,
   2},
  {SettingId::ComposerFeedbackTypingNotifications,
   SettingScope::Config,
   SettingKey::ComposerFeedbackTypingNotifications,
   false},
  {SettingId::ComposerFeedbackReadReceipts,
   SettingScope::Config,
   SettingKey::ComposerFeedbackReadReceipts,
   false},
  {SettingId::ComposerExtrasStickersEnabled,
   SettingScope::Config,
   SettingKey::ComposerExtrasStickersEnabled,
   false},
  {SettingId::NotificationsDesktopEnabled,
   SettingScope::Config,
   SettingKey::NotificationsDesktopEnabled,
   false},
  {SettingId::NotificationsDesktopAlertOnIncoming,
   SettingScope::Config,
   SettingKey::NotificationsDesktopAlertOnIncoming,
   false},
  {SettingId::NotificationsDesktopDecryptMessages,
   SettingScope::Config,
   SettingKey::NotificationsDesktopDecryptMessages,
   false},
  {SettingId::CallsLegacyEnabled, SettingScope::Config, SettingKey::CallsLegacyEnabled, false},
  {SettingId::CallsRelayUseFallbackServer,
   SettingScope::Config,
   SettingKey::CallsRelayUseFallbackServer,
   false},
  {SettingId::CallsDevicesMicrophone,
   SettingScope::Config,
   SettingKey::CallsDevicesMicrophone,
   false},
  {SettingId::CallsDevicesCamera, SettingScope::Config, SettingKey::CallsDevicesCamera, false},
  {SettingId::CallsDevicesCameraResolution,
   SettingScope::Config,
   SettingKey::CallsDevicesCameraResolution,
   false},
  {SettingId::CallsDevicesCameraFrameRate,
   SettingScope::Config,
   SettingKey::CallsDevicesCameraFrameRate,
   false},
  {SettingId::CallsAudioRingtone, SettingScope::Config, SettingKey::CallsAudioRingtone, false},
  {SettingId::TimelineMessagesLayoutBubbles,
   SettingScope::Config,
   SettingKey::TimelineMessagesLayoutBubbles,
   false},
  {SettingId::TimelineMessagesLayoutSmallAvatars,
   SettingScope::Config,
   SettingKey::TimelineMessagesLayoutSmallAvatars,
   false},
  {SettingId::TimelineMessagesLayoutShowOwnAvatar,
   SettingScope::Config,
   SettingKey::TimelineMessagesLayoutShowOwnAvatar,
   false},
  {SettingId::TimelineMessagesSenderUsername,
   SettingScope::Config,
   SettingKey::TimelineMessagesSenderUsername,
   false,
   true,
   0,
   2},
  {SettingId::TimelineMessagesMaxWidthPx,
   SettingScope::Config,
   SettingKey::TimelineMessagesMaxWidthPx,
   false,
   true,
   0,
   20000},
  {SettingId::TimelineMessagesEmojiOnlyEnlarge,
   SettingScope::Config,
   SettingKey::TimelineMessagesEmojiOnlyEnlarge,
   false},
  {SettingId::TimelineMessagesHoverHighlight,
   SettingScope::Config,
   SettingKey::TimelineMessagesHoverHighlight,
   false},
  {SettingId::TimelineMessageActionsEnabled,
   SettingScope::Config,
   SettingKey::TimelineMessageActionsEnabled,
   false},
  {SettingId::TimelineMessageActionsPinnedReactions,
   SettingScope::Config,
   SettingKey::TimelineMessageActionsPinnedReactions,
   false},
  {SettingId::TimelineMediaEffectsEnabled,
   SettingScope::Config,
   SettingKey::TimelineMediaEffectsEnabled,
   false},
  {SettingId::TimelineMediaAnimateOnHover,
   SettingScope::Config,
   SettingKey::TimelineMediaAnimateOnHover,
   false},
  {SettingId::TimelineMediaImageDisplay,
   SettingScope::Config,
   SettingKey::TimelineMediaImageDisplay,
   false,
   true,
   0,
   2},
  {SettingId::TimelineMediaOpenImagesExternal,
   SettingScope::Config,
   SettingKey::TimelineMediaOpenImagesExternal,
   false},
  {SettingId::TimelineMediaOpenVideosExternal,
   SettingScope::Config,
   SettingKey::TimelineMediaOpenVideosExternal,
   false},
  {SettingId::EncryptionKeySharingOnlyVerifiedUsers,
   SettingScope::Config,
   SettingKey::EncryptionKeySharingOnlyVerifiedUsers,
   false},
  {SettingId::EncryptionKeySharingShareWithTrusted,
   SettingScope::Config,
   SettingKey::EncryptionKeySharingShareWithTrusted,
   false},
  {SettingId::EncryptionBackupOnlineEnabled,
   SettingScope::Config,
   SettingKey::EncryptionBackupOnlineEnabled,
   false},
}};

[[nodiscard]] constexpr std::span<const SettingDefinition>
persistedDefinitions()
{
    return kPersistedSettingDefinitions;
}

[[nodiscard]] constexpr std::optional<SettingDefinition>
persistedDefinitionFor(SettingId id)
{
    for (const auto &definition : kPersistedSettingDefinitions) {
        if (definition.id == id)
            return definition;
    }

    return std::nullopt;
}

[[nodiscard]] constexpr bool
hasPersistedDefinition(SettingId id)
{
    return persistedDefinitionFor(id).has_value();
}

[[nodiscard]] constexpr bool
hasUniquePersistedDefinitionIds()
{
    for (std::size_t i = 0; i < kPersistedSettingDefinitions.size(); ++i) {
        for (std::size_t j = i + 1; j < kPersistedSettingDefinitions.size(); ++j) {
            if (kPersistedSettingDefinitions[i].id == kPersistedSettingDefinitions[j].id)
                return false;
        }
    }

    return true;
}

static_assert(hasUniquePersistedDefinitionIds(),
              "settings::core::definitions has duplicate SettingId entries");

[[nodiscard]] constexpr int
normalizeRoomListWidthPx(int value)
{
    return value > 0 ? value : kDefaultSidebarsRoomListWidthPx;
}

[[nodiscard]] constexpr int
normalizeCommunitiesWidthPx(int value)
{
    return value > 0 ? value : kDefaultSidebarsCommunitiesWidthPx;
}

} // namespace settings::core::definitions
