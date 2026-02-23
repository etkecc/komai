// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>

#include <yaml-cpp/yaml.h>

#include <QString>

namespace settings::startup {

/**
 * Startup snapshot read before QApplication construction.
 */
struct StartupSettings
{
    YAML::Node configRoot;
    std::optional<float> uiScaleFactor;
};

/**
 * Read startup-only values once from profile config.
 */
StartupSettings
readStartupConfig(const QString &profile);

} // namespace settings::startup
