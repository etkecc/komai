// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>

namespace settings::core {

constexpr float kMinScaleFactor  = 0.0F;
constexpr float kMaxScaleFactor  = 3.0F;
constexpr float kScaleFactorStep = 0.25F;

inline bool
isScaleFactorInRange(float factor)
{
    return factor >= kMinScaleFactor && factor <= kMaxScaleFactor;
}

/*!
 * Snapshot of bootstrap settings loaded before the Qt application is initialized.
 *
 * This model intentionally stays dependency-light (no QObject/QML types), so it can be
 * constructed and queried as part of app startup before the full user settings object exists.
 */
struct StartupConfigSnapshot
{
    /// Scale factor from config.  0.0 means auto-detect (do not set QT_SCALE_FACTOR).
    float uiScaleFactor = 0.0F;
};

StartupConfigSnapshot
snapshotForProfile(std::string_view profileId);

} // namespace settings::core
