// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsMigrations.h"

#include <yaml-cpp/yaml.h>

#include "settings/SettingKeys.h"
#include "settings/YamlSettings.h"

namespace settings::migrations {

namespace {

int
readConfigSchemaVersion(const YAML::Node &configRoot)
{
    return yaml_settings::readScalar<int>(configRoot, SettingKey::ConfigSchemaVersion, 0);
}

} // namespace

void
stampCurrentConfigSchemaVersion(YAML::Node &configRoot)
{
    if (!configRoot || !configRoot.IsMap())
        configRoot = YAML::Node(YAML::NodeType::Map);
    yaml_settings::setNode(
      configRoot, SettingKey::ConfigSchemaVersion, kCurrentConfigSchemaVersion);
}

ConfigMigrationOutcome
migrateConfigRoot(const YAML::Node &configRoot)
{
    ConfigMigrationOutcome outcome;
    outcome.migratedRoot  = (configRoot && configRoot.IsMap()) ? YAML::Clone(configRoot)
                                                               : YAML::Node(YAML::NodeType::Map);
    outcome.sourceVersion = readConfigSchemaVersion(outcome.migratedRoot);
    if (outcome.sourceVersion > kCurrentConfigSchemaVersion) {
        outcome.hadFutureVersion = true;
        return outcome;
    }

    // v0 -> v1: foundational schema version stamping (no key rewrites yet).
    stampCurrentConfigSchemaVersion(outcome.migratedRoot);
    return outcome;
}

} // namespace settings::migrations
