// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/facade/UserSettingsCoreStoreBridge.h"

#include <array>

#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::ui::facade {

namespace {

constexpr auto kMappedSettingIds = std::to_array<settings::core::SettingId>({
#define X(id, expr) settings::core::SettingId::id,
#include "settings/ui/facade/UserSettingsCoreStoreBridgeEntries.inc"
#undef X
});

constexpr bool
hasUniqueMappedSettingIds()
{
    for (std::size_t i = 0; i < kMappedSettingIds.size(); ++i) {
        for (std::size_t j = i + 1; j < kMappedSettingIds.size(); ++j) {
            if (kMappedSettingIds[i] == kMappedSettingIds[j])
                return false;
        }
    }

    return true;
}

constexpr bool
allPersistedDefinitionsMapped()
{
    const auto definitions = settings::core::definitions::persistedDefinitions();
    for (const auto &definition : definitions) {
        bool found = false;
        for (const auto id : kMappedSettingIds) {
            if (id == definition.id) {
                found = true;
                break;
            }
        }

        if (!found)
            return false;
    }

    return true;
}

static_assert(hasUniqueMappedSettingIds(),
              "settings core-store bridge contains duplicate mapping entries");
static_assert(allPersistedDefinitionsMapped(),
              "settings core-store bridge misses one or more persisted definitions");

} // namespace

std::optional<settings::core::SettingsStore::Value>
coreStoreValueForSettingId(const UserSettings &settings, settings::core::SettingId id)
{
    switch (id) {
#define X(id, expr)                                                                                \
    case settings::core::SettingId::id:                                                            \
        return expr;
#include "settings/ui/facade/UserSettingsCoreStoreBridgeEntries.inc"
#undef X
    default:
        return std::nullopt;
    }
}

bool
hasCoreStoreValueMapping(settings::core::SettingId id)
{
    switch (id) {
#define X(id, expr)                                                                                \
    case settings::core::SettingId::id:                                                            \
        return true;
#include "settings/ui/facade/UserSettingsCoreStoreBridgeEntries.inc"
#undef X
    default:
        return false;
    }
}

} // namespace settings::ui::facade
