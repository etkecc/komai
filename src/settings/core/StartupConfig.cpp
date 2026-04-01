// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StartupConfig.h"

#include "settings/SettingsStorage.h"
#include <yaml-cpp/yaml.h>

namespace {

settings::core::StartupConfigSnapshot
snapshotFromYamlNode(const YAML::Node &configRoot)
{
    settings::core::StartupConfigSnapshot snapshot;

    try {
        if (configRoot.IsDefined() && configRoot["ui"]["scale"]["factor"].IsDefined()) {
            const auto factor      = configRoot["ui"]["scale"]["factor"].as<float>(-1.0F);
            snapshot.uiScaleFactor = settings::core::normalizeScaleFactor(factor);
        }
    } catch (...) {
        // Ignore malformed UI scale factor values.
    }

    return snapshot;
}

} // namespace

namespace settings::core {

StartupConfigSnapshot
snapshotFromYamlFile(std::string_view path)
{
    try {
        return snapshotFromYamlNode(settings::storage::loadYamlFile(
          QString::fromStdString(std::string(path)), "startup config"));
    } catch (...) {
        return {};
    }
}

} // namespace settings::core
