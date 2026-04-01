// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

#include <optional>

namespace settings::startup {

/**
 * Startup snapshot read before QApplication construction.
 */
struct StartupSettings
{
    std::optional<float> uiScaleFactor;
};

/**
 * Read startup-only values once from profile config.
 */
StartupSettings
readStartupConfig(const QString &profile);

} // namespace settings::startup
