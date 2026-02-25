// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <yaml-cpp/yaml.h>

namespace settings::migrations {

struct ConfigMigrationOutcome
{
    YAML::Node migratedRoot;
    int sourceVersion     = 0;
    bool hadFutureVersion = false;
};

inline constexpr int kCurrentConfigSchemaVersion = 1;

ConfigMigrationOutcome
migrateConfigRoot(const YAML::Node &configRoot);
void
stampCurrentConfigSchemaVersion(YAML::Node &configRoot);

} // namespace settings::migrations
