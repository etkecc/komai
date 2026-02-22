// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StartupConfig.h"

#include <fstream>
#include <filesystem>

namespace {

} // namespace

namespace settings::core {

StartupConfigSnapshot
snapshotFromYamlConfig(const YAML::Node &configRoot)
{
    StartupConfigSnapshot snapshot;
    snapshot.configRoot = configRoot;

    try {
        if (configRoot.IsDefined() && configRoot["ui"]["scale"]["factor"].IsDefined()) {
            const auto factor = configRoot["ui"]["scale"]["factor"].as<float>(-1.0F);
            snapshot.uiScaleFactor = normalizeScaleFactor(factor);
        }
    } catch (...) {
        // Ignore malformed UI scale factor values.
    }

    return snapshot;
}

StartupConfigSnapshot
snapshotFromYamlFile(std::string_view path)
{
    const std::filesystem::path filePath(path);
    if (!std::filesystem::exists(filePath))
        return {};

    try {
        return snapshotFromYamlConfig(YAML::LoadFile(filePath.string()));
    } catch (...) {
        return {};
    }
}

} // namespace settings::core
