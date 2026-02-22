// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StartupSettings.h"

#include <QFileInfo>

#include <yaml-cpp/yaml.h>

#include "Paths.h"
#include "settings/core/StartupConfig.h"

namespace settings::startup {

StartupSettings
readStartupConfig(const QString &profile)
{
    StartupSettings settings;
    const auto path = app_paths::config::profileConfigFile(profile);
    if (!QFileInfo::exists(path))
        return settings;

    try {
        const auto root =
          settings::core::snapshotFromYamlConfig(YAML::LoadFile(path.toStdString()));
        settings.configRoot    = root.configRoot;
        settings.uiScaleFactor = root.uiScaleFactor;
    } catch (const YAML::Exception &) {
    }

    return settings;
}

} // namespace settings::startup
