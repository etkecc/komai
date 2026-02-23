// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include "settings/core/SettingDefinition.h"
#include "settings/core/SettingsStore.h"

class UserSettings;

namespace settings::ui::facade {

[[nodiscard]] std::optional<settings::core::SettingsStore::Value>
coreStoreValueForSettingId(const UserSettings &settings, settings::core::SettingId id);

[[nodiscard]] bool
hasCoreStoreValueMapping(settings::core::SettingId id);

} // namespace settings::ui::facade
