// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StartupSettings.h"

#include "Paths.h"
#include "settings/core/StartupConfig.h"

namespace settings::startup {

StartupSettings
readStartupConfig(const QString &profile)
{
    const auto path = app_paths::config::profileConfigFile(profile);
    const auto root = settings::core::snapshotFromYamlFile(path.toStdString());
    return StartupSettings{.configRoot = root.configRoot, .uiScaleFactor = root.uiScaleFactor};
}

} // namespace settings::startup
