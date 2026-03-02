// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <utility>

#include "Logging.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsController.h"
#include "settings/ui/facade/UserSettingsPage.h"

bool
UserSettings::setCoreValue(settings::core::SettingId id,
                           settings::core::SettingsStore::Value value,
                           const char *settingName)
{
    const auto result = coreStore_.setValue(id, std::move(value));
    if (result.success)
        return true;

    nhlog::ui()->warn("Ignoring invalid settings update for '{}': {}",
                      settingName != nullptr ? settingName : "(unknown)",
                      result.validationError);
    return false;
}

#include "UserSettingsSettersCore.inc"
