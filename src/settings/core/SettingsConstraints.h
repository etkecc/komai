// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <optional>

#include "settings/core/SettingsDefinitions.h"
#include "settings/core/SettingsStore.h"

namespace settings::core::constraints {

[[nodiscard]] constexpr std::optional<SettingsStore::IntRange>
intRangeConstraintFor(SettingId id)
{
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.id != id || !definition.hasIntRangeConstraint)
            continue;

        return SettingsStore::IntRange{.minValue = definition.intRangeConstraintMin,
                                       .maxValue = definition.intRangeConstraintMax};
    }

    return std::nullopt;
}

[[nodiscard]] constexpr bool
hasIntRangeConstraint(SettingId id)
{
    return intRangeConstraintFor(id).has_value();
}

[[nodiscard]] constexpr bool
hasValidIntRangeConstraints()
{
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (!definition.hasIntRangeConstraint)
            continue;

        if (definition.intRangeConstraintMin > definition.intRangeConstraintMax)
            return false;
    }

    return true;
}

[[nodiscard]] constexpr bool
hasNoUnknownConstrainedSettings()
{
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.hasIntRangeConstraint && definition.id == SettingId::Unknown)
            return false;
    }

    return true;
}

[[nodiscard]] constexpr std::size_t
intRangeConstraintCount()
{
    std::size_t count = 0;
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.hasIntRangeConstraint)
            ++count;
    }
    return count;
}

static_assert(hasValidIntRangeConstraints(),
              "settings::core::constraints contains invalid integer ranges");
static_assert(hasNoUnknownConstrainedSettings(),
              "settings::core::constraints includes an Unknown setting id");

inline void
applyDefaultConstraints(SettingsStore &store)
{
    store.clearConstraints();

    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (!definition.hasIntRangeConstraint)
            continue;

        store.setIntRangeConstraint(
          definition.id, definition.intRangeConstraintMin, definition.intRangeConstraintMax);
    }
}

} // namespace settings::core::constraints
