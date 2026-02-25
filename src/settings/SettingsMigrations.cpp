// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsMigrations.h"

#include <algorithm>

#include <yaml-cpp/yaml.h>

#include "settings/SettingKeys.h"
#include "settings/YamlSettings.h"

namespace settings::migrations {

namespace {

int
readConfigSchemaVersion(const YAML::Node &configRoot)
{
    return std::max(0,
                    yaml_settings::readScalar<int>(configRoot, SettingKey::ConfigSchemaVersion, 0));
}

void
migrateConfigV0ToV1(YAML::Node &configRoot)
{
    // v0 -> v1: foundational schema version stamping (no key rewrites yet).
    stampCurrentConfigSchemaVersion(configRoot);
}

bool
applyMigrationStep(int sourceVersion, YAML::Node &configRoot)
{
    switch (sourceVersion) {
    case 0:
        migrateConfigV0ToV1(configRoot);
        return true;
    default:
        return false;
    }
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
    outcome.migratedRoot    = (configRoot && configRoot.IsMap()) ? YAML::Clone(configRoot)
                                                                 : YAML::Node(YAML::NodeType::Map);
    outcome.sourceVersion   = readConfigSchemaVersion(outcome.migratedRoot);
    outcome.migratedVersion = outcome.sourceVersion;
    if (outcome.sourceVersion > kCurrentConfigSchemaVersion) {
        outcome.hadFutureVersion = true;
        return outcome;
    }

    while (outcome.migratedVersion < kCurrentConfigSchemaVersion) {
        const bool applied = applyMigrationStep(outcome.migratedVersion, outcome.migratedRoot);
        if (!applied) {
            outcome.hadUnsupportedPath = true;
            break;
        }
        ++outcome.migratedVersion;
    }

    if (!outcome.hadUnsupportedPath) {
        yaml_settings::setNode(
          outcome.migratedRoot, SettingKey::ConfigSchemaVersion, outcome.migratedVersion);
    }

    return outcome;
}

} // namespace settings::migrations
