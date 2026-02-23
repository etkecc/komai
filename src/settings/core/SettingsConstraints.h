// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <optional>
#include <span>

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

[[nodiscard]] inline std::span<const IntRangeConstraint>
intRangeConstraints()
{
    return kIntRangeConstraints;
}

[[nodiscard]] inline std::optional<SettingsStore::IntRange>
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

[[nodiscard]] inline bool
hasIntRangeConstraint(SettingId id)
{
    return intRangeConstraintFor(id).has_value();
}

inline void
applyDefaultConstraints(SettingsStore &store)
{
    store.clearConstraints();

    for (const auto &constraint : kIntRangeConstraints) {
        store.setIntRangeConstraint(constraint.id, constraint.minValue, constraint.maxValue);
    }
}

} // namespace settings::core::constraints
