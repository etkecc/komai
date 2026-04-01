// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "settings/SettingsSchemaVersions.h"

#include <yaml-cpp/yaml.h>

namespace settings::migrations {

struct ScopeMigrationOutcome
{
    YAML::Node migratedRoot;
    int sourceVersion       = 0;
    int migratedVersion     = 0;
    bool hadFutureVersion   = false;
    bool hadUnsupportedPath = false;
};

using ConfigMigrationOutcome  = ScopeMigrationOutcome;
using StateMigrationOutcome   = ScopeMigrationOutcome;
using SessionMigrationOutcome = ScopeMigrationOutcome;

ConfigMigrationOutcome
migrateConfigRoot(const YAML::Node &configRoot);
StateMigrationOutcome
migrateStateRoot(const YAML::Node &stateRoot);
SessionMigrationOutcome
migrateSessionRoot(const YAML::Node &sessionRoot);
void
stampCurrentConfigSchemaVersion(YAML::Node &configRoot);
void
stampCurrentStateSchemaVersion(YAML::Node &stateRoot);
void
stampCurrentSessionSchemaVersion(YAML::Node &sessionRoot);

} // namespace settings::migrations
