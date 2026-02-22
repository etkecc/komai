// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StartupSettings.h"

#include "settings/SettingsStorage.h"
#include "settings/core/StartupConfig.h"

namespace settings::startup {

StartupSettings
readStartupConfig(const QString &profile)
{
    const auto path = settings::storage::configFilePathForProfile(profile);
    const auto root = settings::core::snapshotFromYamlConfig(
      settings::storage::loadYamlFile(path, "startup config"));
    return StartupSettings{.configRoot = root.configRoot, .uiScaleFactor = root.uiScaleFactor};
}

} // namespace settings::startup
