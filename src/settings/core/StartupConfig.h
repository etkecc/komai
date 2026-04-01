// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>

namespace settings::core {

constexpr float kMinScaleFactor  = 1.0F;
constexpr float kMaxScaleFactor  = 3.0F;
constexpr float kScaleFactorStep = 0.25F;

inline bool
isScaleFactorInRange(float factor)
{
    return factor >= kMinScaleFactor && factor <= kMaxScaleFactor;
}

inline std::optional<float>
normalizeScaleFactor(float factor)
{
    return isScaleFactorInRange(factor) ? std::optional<float>{factor} : std::nullopt;
}

/*!
 * Snapshot of bootstrap settings loaded before the Qt application is initialized.
 *
 * This model intentionally stays dependency-light (no QObject/QML types), so it can be
 * constructed and queried as part of app startup before the full user settings object exists.
 */
struct StartupConfigSnapshot
{
    std::optional<float> uiScaleFactor;
};

/**
 * Parse startup configuration snapshot from a YAML file path.
 *
 * Non-Qt helper used during bootstrap, before a QObject/QML stack exists.
 */
StartupConfigSnapshot
snapshotFromYamlFile(std::string_view path);

} // namespace settings::core
