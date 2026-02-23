// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <optional>
#include <span>

#include "settings/core/SettingsDefinitions.h"
#include "settings/core/SettingsStore.h"

namespace settings::core::constraints {

struct IntRangeConstraint
{
    SettingId id{SettingId::Unknown};
    int minValue{0};
    int maxValue{0};
};

inline constexpr std::array<IntRangeConstraint, 10> kIntRangeConstraints{{
  {SettingId::IntegrationsDbusApiAccess, 0, 2},
  {SettingId::NetworkPresenceStatusPolicy, 0, 3},
  {SettingId::ComposerInputSendKey, 0, 2},
  {SettingId::ComposerInputAutoReplaceEmoji, 0, 2},
  {SettingId::SidebarsRoomListSort, 0, 3},
  {SettingId::SidebarsRoomListLastMessagePreview, 0, 2},
  {SettingId::TimelineMessagesSenderUsername, 0, 2},
  {SettingId::TimelineMediaImageDisplay, 0, 2},
  {SettingId::TimelineMessagesMaxWidthPx, 0, 20000},
  {SettingId::PrivacyScreenLockTimeoutSeconds, 0, 3600},
}};

[[nodiscard]] constexpr std::span<const IntRangeConstraint>
intRangeConstraints()
{
    return kIntRangeConstraints;
}

[[nodiscard]] constexpr std::optional<SettingsStore::IntRange>
intRangeConstraintFor(SettingId id)
{
    for (const auto &constraint : kIntRangeConstraints) {
        if (constraint.id == id) {
            return SettingsStore::IntRange{.minValue = constraint.minValue,
                                           .maxValue = constraint.maxValue};
        }
    }

    return std::nullopt;
}

[[nodiscard]] constexpr bool
hasIntRangeConstraint(SettingId id)
{
    return intRangeConstraintFor(id).has_value();
}

[[nodiscard]] constexpr bool
hasUniqueIntRangeConstraintIds()
{
    for (std::size_t i = 0; i < kIntRangeConstraints.size(); ++i) {
        for (std::size_t j = i + 1; j < kIntRangeConstraints.size(); ++j) {
            if (kIntRangeConstraints[i].id == kIntRangeConstraints[j].id)
                return false;
        }
    }

    return true;
}

[[nodiscard]] constexpr bool
allConstrainedSettingsHavePersistedDefinitions()
{
    for (const auto &constraint : kIntRangeConstraints) {
        if (!settings::core::definitions::hasPersistedDefinition(constraint.id))
            return false;
    }

    return true;
}

static_assert(hasUniqueIntRangeConstraintIds(),
              "settings::core::constraints has duplicate SettingId entries");
static_assert(
  allConstrainedSettingsHavePersistedDefinitions(),
  "settings::core::constraints contains SettingIds not present in persisted definitions");

inline void
applyDefaultConstraints(SettingsStore &store)
{
    store.clearConstraints();

    for (const auto &constraint : kIntRangeConstraints) {
        store.setIntRangeConstraint(constraint.id, constraint.minValue, constraint.maxValue);
    }
}

} // namespace settings::core::constraints
