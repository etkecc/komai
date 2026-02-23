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

inline constexpr int kDefaultSidebarsRoomListWidthPx            = 300;
inline constexpr int kDefaultSidebarsCommunitiesWidthPx         = 200;
inline constexpr int kDefaultWindowWidthPx                      = 1050;
inline constexpr int kDefaultWindowHeightPx                     = 700;
inline constexpr const char *kDefaultUiThemeSlug                = "komai-light";
inline constexpr const char *kDefaultCallsAudioRingtone         = "Default";
inline constexpr const char *kDefaultPinnedReactions            = "👍️,👎️,😀,🤣,❤️";
inline constexpr bool kDefaultUiMotionAnimationsEnabled         = true;
inline constexpr bool kDefaultUiInputModeTouchEnabled           = false;
inline constexpr bool kDefaultCertificateValidationEnabled      = true;
inline constexpr bool kDefaultNetworkHttp3Enabled               = false;
inline constexpr double kDefaultScaleFactor                     = -1.0;
inline constexpr double kDefaultFontSizePt                      = 13.0;
inline constexpr int kDefaultScreenShareFrameRate               = 5;
inline constexpr bool kDefaultScreenShareShowCursor             = true;
inline constexpr int kDefaultPrivacyWindowFocusBlurDelaySeconds = 0;
inline constexpr int kDefaultTimelineMaxWidthPx                 = 0;
inline constexpr unsigned int kDefaultMaxStores                 = 0;
inline constexpr unsigned long long kDefaultMaxDbSizeBytes      = 0;
inline constexpr int kDefaultIntegrationsDbusApiAccess          = 0;

inline constexpr std::array<SettingDefinition, 65> kPersistedSettingDefinitions{{
  {SettingId::UiThemeSlug, SettingScope::Config, SettingKey::UiThemeSlug, false},
  {SettingId::UiFontFamily, SettingScope::Config, SettingKey::UiFontFamily, false},
  {SettingId::UiFontSizePt, SettingScope::Config, SettingKey::UiFontSizePt, false},
  {SettingId::UiFontEmojiFamily, SettingScope::Config, SettingKey::UiFontEmojiFamily, false},
  {SettingId::UiMotionAnimationsEnabled,
   SettingScope::Config,
   SettingKey::UiMotionAnimationsEnabled,
   false},
  {SettingId::UiInputMode, SettingScope::Config, SettingKey::UiInputMode, false},
  {SettingId::UiInputTouchSwipeGesturesEnabled,
   SettingScope::Config,
   SettingKey::UiInputTouchSwipeGesturesEnabled,
   false},
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
  {SettingId::NetworkTlsEnableCertificateValidation,
   SettingScope::Config,
   SettingKey::NetworkTlsEnableCertificateValidation,
   false},
  {SettingId::NetworkHttp3Enabled, SettingScope::Config, SettingKey::NetworkHttp3Enabled, false},
  {SettingId::PrivacyMaintenanceExpireEvents,
   SettingScope::Config,
   SettingKey::PrivacyMaintenanceExpireEvents,
   false},
  {SettingId::PrivacyMaintenanceUpdateSpaceVias,
   SettingScope::Config,
   SettingKey::PrivacyMaintenanceUpdateSpaceVias,
   false},
  {SettingId::PrivacyWindowFocusBlurEnabled,
   SettingScope::Config,
   SettingKey::PrivacyWindowFocusBlurEnabled,
   false},
  {SettingId::PrivacyWindowFocusBlurDelaySeconds,
   SettingScope::Config,
   SettingKey::PrivacyWindowFocusBlurDelaySeconds,
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
  {SettingId::ComposerTypingSendEnabled,
   SettingScope::Config,
   SettingKey::ComposerTypingSendEnabled,
   false},
  {SettingId::ComposerExtrasStickersEnabled,
   SettingScope::Config,
   SettingKey::ComposerExtrasStickersEnabled,
   false},
  {SettingId::NotificationsEnabled, SettingScope::Config, SettingKey::NotificationsEnabled, false},
  {SettingId::NotificationsAttentionOnIncoming,
   SettingScope::Config,
   SettingKey::NotificationsAttentionOnIncoming,
   false},
  {SettingId::NotificationsMessageContentPolicy,
   SettingScope::Config,
   SettingKey::NotificationsMessageContentPolicy,
   false,
   true,
   0,
   2},
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
  {SettingId::CallsScreenshareFrameRate,
   SettingScope::Config,
   SettingKey::CallsScreenshareFrameRate,
   false,
   true,
   1,
   120},
  {SettingId::CallsScreensharePictureInPicture,
   SettingScope::Config,
   SettingKey::CallsScreensharePictureInPicture,
   false},
  {SettingId::CallsScreenshareIncludeRemoteVideo,
   SettingScope::Config,
   SettingKey::CallsScreenshareIncludeRemoteVideo,
   false},
  {SettingId::CallsScreenshareShowCursor,
   SettingScope::Config,
   SettingKey::CallsScreenshareShowCursor,
   false},
  {SettingId::TimelineMessagesLayoutStyle,
   SettingScope::Config,
   SettingKey::TimelineMessagesLayoutStyle,
   false,
   true,
   0,
   1},
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
  {SettingId::TimelineTypingShowEnabled,
   SettingScope::Config,
   SettingKey::TimelineTypingShowEnabled,
   false},
  {SettingId::TimelineReadReceiptsEnabled,
   SettingScope::Config,
   SettingKey::TimelineReadReceiptsEnabled,
   false},
  {SettingId::TimelineMessageActionsActivationPolicy,
   SettingScope::Config,
   SettingKey::TimelineMessageActionsActivationPolicy,
   false,
   true,
   0,
   2},
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

[[nodiscard]] constexpr int
normalizeWindowWidthPx(int value)
{
    return value > 0 ? value : kDefaultWindowWidthPx;
}

[[nodiscard]] constexpr int
normalizeWindowHeightPx(int value)
{
    return value > 0 ? value : kDefaultWindowHeightPx;
}

} // namespace settings::core::definitions
