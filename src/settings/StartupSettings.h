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
    /// Scale factor from config.  0.0 means auto-detect (do not set QT_SCALE_FACTOR).
    float uiScaleFactor = 0.0F;
};

/**
 * Read startup-only values once from profile config.
 */
StartupSettings
readStartupConfig(const QString &profile);

} // namespace settings::startup
