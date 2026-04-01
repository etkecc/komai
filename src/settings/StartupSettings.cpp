// SPDX-FileCopyrightText: Nheko Contributors
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
    const auto root = settings::core::snapshotFromYamlFile(path.toStdString());
    return StartupSettings{.uiScaleFactor = root.uiScaleFactor};
}

} // namespace settings::startup
