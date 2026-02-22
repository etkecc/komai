// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "StartupConfig.h"

namespace {

constexpr float kMinScaleFactor = 1.0F;
constexpr float kMaxScaleFactor = 3.0F;

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
            if (factor >= kMinScaleFactor && factor <= kMaxScaleFactor)
                snapshot.uiScaleFactor = factor;
        }
    } catch (...) {
        // Ignore malformed UI scale factor values.
    }

    return snapshot;
}

} // namespace settings::core
