// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/facade/UserSettingsCoreStoreBridge.h"

#include "settings/ui/facade/UserSettingsPage.h"

namespace settings::ui::facade {

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
