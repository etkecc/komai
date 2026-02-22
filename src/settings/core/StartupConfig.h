// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <yaml-cpp/yaml.h>

namespace settings::core {

/*!
 * Snapshot of bootstrap settings loaded before the Qt application is initialized.
 *
 * This model intentionally stays dependency-light (no QObject/QML types), so it can be
 * constructed and queried as part of app startup before the full user settings object exists.
 */
struct StartupConfigSnapshot
{
    std::optional<float> uiScaleFactor;
    YAML::Node configRoot;
};

StartupConfigSnapshot
snapshotFromYamlConfig(const YAML::Node &configRoot);

} // namespace settings::core
