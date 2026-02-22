// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BootstrapSettings.h"

#include <QFileInfo>

#include <yaml-cpp/yaml.h>

#include "Paths.h"
#include "settings/SettingKeys.h"
#include "settings/YamlSettings.h"

namespace settings::bootstrap {

std::optional<float>
readUiScaleFactor(QStringView profile)
{
    const auto path = app_paths::config::profileConfigFile(profile);
    if (!QFileInfo::exists(path))
        return std::nullopt;

    try {
        const auto root = YAML::LoadFile(path.toStdString());
        const auto factor = yaml_settings::readScalar<float>(root, SettingKey::UiScaleFactor, -1.0F);
        if (factor < 1.0F || factor > 3.0F)
            return std::nullopt;
        return factor;
    } catch (const YAML::Exception &) {
        return std::nullopt;
    }
}

} // namespace settings::bootstrap

