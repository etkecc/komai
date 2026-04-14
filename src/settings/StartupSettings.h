// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>

namespace settings::startup {

/**
 * Startup snapshot read before QApplication construction.
 */
struct StartupSettings
{
    /// Additional scale multiplier applied via QT_SCALE_FACTOR at startup.
    float uiScaleFactor = 1.0F;
};

/**
 * Read startup-only values once from profile config.
 */
StartupSettings
readStartupConfig(const QString &profile);

} // namespace settings::startup
