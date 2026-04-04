// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "settings/SettingKeys.h"
#include "settings/core/SettingDefinition.h"

namespace settings::core::definitions {

inline constexpr int kDefaultSidebarsRoomListWidthPx             = 400;
inline constexpr int kDefaultSidebarsCommunitiesWidthPx          = 220;
inline constexpr int kDefaultWindowWidthPx                       = 1600;
inline constexpr int kDefaultWindowHeightPx                      = 900;
inline constexpr const char *kDefaultUiThemeSlug                 = "light-komai";
inline constexpr const char *kDefaultCallsAudioRingtone          = "Default";
inline constexpr const char *kDefaultPinnedReactions             = "👍,👎,😀,❤️";
inline constexpr bool kDefaultUiMotionAnimationsEnabled          = true;
inline constexpr int kMaxQuickReactionSlots                      = 8;
inline constexpr int kReactionFrequencyLookbackDays              = 30;
inline constexpr int kReactionFrequencyCacheDurationMs           = 60'000;
inline constexpr uint64_t kMaxReactionScanEvents                 = 5000;
inline constexpr bool kDefaultUiInputMode                        = false;
inline constexpr bool kDefaultCertificateValidationEnabled       = true;
inline constexpr bool kDefaultNetworkHttp3Enabled                = false;
inline constexpr bool kDefaultNetworkMrsEnabled                  = true;
inline constexpr const char *kDefaultNetworkMrsServerName        = "matrixrooms.info";
inline constexpr double kDefaultScaleFactor                      = -1.0;
inline constexpr double kDefaultFontSizePt                       = 13.0;
inline constexpr double kDefaultTimelineMediaAudioPlaybackSpeed  = 1.0;
inline constexpr double kMinTimelineMediaAudioPlaybackSpeed      = 0.5;
inline constexpr double kMaxTimelineMediaAudioPlaybackSpeed      = 3.0;
inline constexpr double kTimelineMediaAudioPlaybackSpeedStep     = 0.5;
inline constexpr int kDefaultUiLayoutContentMaxWidthPx           = 0;
inline constexpr int kMinEffectiveUiLayoutContentMaxWidthPx      = 500;
inline constexpr int kDefaultScreenShareFrameRate                = 30;
inline constexpr bool kDefaultScreenShareShowCursor              = true;
inline constexpr bool kDefaultDesktopAttentionWindowTitleEnabled = true;
inline constexpr bool kDefaultDesktopAttentionAppBadgeEnabled    = true;
inline constexpr int kDefaultDesktopWindowFocusBlurDelaySeconds  = 0;
inline constexpr int kDefaultIntegrationsDbusApiAccess           = 0;

inline constexpr SettingId kEnumTokenConfigSettingIds[] = {
#include "SettingsDefinitionsEnumTokenConfigSettingIds.inc"
};

inline constexpr SettingId kNumericConstrainedConfigSettingIds[] = {
#include "SettingsDefinitionsNumericConstrainedConfigSettingIds.inc"
};

inline constexpr SettingDefinition kPersistedSettingDefinitions[] = {
#include "SettingsDefinitionsPersistedCalls.inc"
#include "SettingsDefinitionsPersistedComposer.inc"
#include "SettingsDefinitionsPersistedDesktop.inc"
#include "SettingsDefinitionsPersistedEncryption.inc"
#include "SettingsDefinitionsPersistedIntegrations.inc"
#include "SettingsDefinitionsPersistedNetwork.inc"
#include "SettingsDefinitionsPersistedSidebars.inc"
#include "SettingsDefinitionsPersistedTimeline.inc"
#include "SettingsDefinitionsPersistedUi.inc"
};

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
    for (std::size_t i = 0; i < std::size(kPersistedSettingDefinitions); ++i) {
        for (std::size_t j = i + 1; j < std::size(kPersistedSettingDefinitions); ++j) {
            if (kPersistedSettingDefinitions[i].id == kPersistedSettingDefinitions[j].id)
                return false;
        }
    }

    return true;
}

static_assert(hasUniquePersistedDefinitionIds(),
              "settings::core::definitions has duplicate SettingId entries");

[[nodiscard]] constexpr std::span<const SettingId>
enumTokenConfigSettingIds()
{
    return kEnumTokenConfigSettingIds;
}

[[nodiscard]] constexpr bool
isEnumTokenConfigSettingId(SettingId id)
{
    for (const auto candidate : kEnumTokenConfigSettingIds) {
        if (candidate == id)
            return true;
    }
    return false;
}

[[nodiscard]] constexpr bool
isNumericConstrainedConfigSettingId(SettingId id)
{
    for (const auto candidate : kNumericConstrainedConfigSettingIds) {
        if (candidate == id)
            return true;
    }
    return false;
}

[[nodiscard]] constexpr bool
hasCompleteConstrainedConfigClassification()
{
    for (const auto &definition : kPersistedSettingDefinitions) {
        if (!definition.hasIntRangeConstraint || definition.scope != SettingScope::Config)
            continue;

        if (!isEnumTokenConfigSettingId(definition.id) &&
            !isNumericConstrainedConfigSettingId(definition.id))
            return false;
    }

    for (const auto id : kEnumTokenConfigSettingIds) {
        if (!hasPersistedDefinition(id))
            return false;
    }

    for (const auto id : kNumericConstrainedConfigSettingIds) {
        if (!hasPersistedDefinition(id))
            return false;
    }

    return true;
}

static_assert(hasCompleteConstrainedConfigClassification(),
              "constrained config settings must be classified as enum-token or numeric");

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

[[nodiscard]] constexpr int
normalizeUiLayoutContentMaxWidthPx(int value)
{
    return value > 0 ? value : kDefaultUiLayoutContentMaxWidthPx;
}

[[nodiscard]] constexpr int
effectiveUiLayoutContentMaxWidthPx(int value)
{
    const auto normalized = normalizeUiLayoutContentMaxWidthPx(value);
    if (normalized <= 0)
        return normalized;

    return normalized < kMinEffectiveUiLayoutContentMaxWidthPx
             ? kMinEffectiveUiLayoutContentMaxWidthPx
             : normalized;
}

} // namespace settings::core::definitions
