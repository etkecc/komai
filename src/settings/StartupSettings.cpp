// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StartupSettings.h"

#include <QFileInfo>

#include <yaml-cpp/yaml.h>

#include "Paths.h"
#include "settings/SettingKeys.h"
#include "settings/YamlSettings.h"

namespace settings::startup {

StartupSettings
readStartupConfig(const QString &profile)
{
    StartupSettings settings;
    const auto path = app_paths::config::profileConfigFile(profile);
    if (!QFileInfo::exists(path))
        return settings;

    try {
        settings.configRoot = YAML::LoadFile(path.toStdString());
        const auto factor =
          yaml_settings::readScalar<float>(settings.configRoot, SettingKey::UiScaleFactor, -1.0F);
        if (factor >= 1.0F && factor <= 3.0F)
            settings.uiScaleFactor = factor;
    } catch (const YAML::Exception &) {
    }

    return settings;
}

} // namespace settings::startup
