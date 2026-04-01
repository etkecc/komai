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
readSchemaVersion(const YAML::Node &root, const char *schemaVersionKey)
{
    return std::max(0, yaml_settings::readScalar<int>(root, schemaVersionKey, 0));
}

void
stampSchemaVersion(YAML::Node &root, const char *schemaVersionKey, int version)
{
    if (!root || !root.IsMap())
        root = YAML::Node(YAML::NodeType::Map);
    yaml_settings::setNode(root, schemaVersionKey, version);
}

bool
applyMigrationStep(int sourceVersion,
                   YAML::Node &root,
                   const char *schemaVersionKey,
                   int currentVersion)
{
    switch (sourceVersion) {
    case 0:
        // v0 -> v1: foundational schema version stamping (no key rewrites yet).
        if (currentVersion >= 1)
            stampSchemaVersion(root, schemaVersionKey, 1);
        return true;
    default:
        return false;
    }
}

ScopeMigrationOutcome
migrateRoot(const YAML::Node &root, const char *schemaVersionKey, int currentVersion)
{
    ScopeMigrationOutcome outcome;
    outcome.migratedRoot =
      (root && root.IsMap()) ? YAML::Clone(root) : YAML::Node(YAML::NodeType::Map);
    outcome.sourceVersion   = readSchemaVersion(outcome.migratedRoot, schemaVersionKey);
    outcome.migratedVersion = outcome.sourceVersion;

    if (outcome.sourceVersion > currentVersion) {
        outcome.hadFutureVersion = true;
        return outcome;
    }

    while (outcome.migratedVersion < currentVersion) {
        const bool applied = applyMigrationStep(
          outcome.migratedVersion, outcome.migratedRoot, schemaVersionKey, currentVersion);
        if (!applied) {
            outcome.hadUnsupportedPath = true;
            break;
        }
        ++outcome.migratedVersion;
    }

    if (!outcome.hadUnsupportedPath)
        stampSchemaVersion(outcome.migratedRoot, schemaVersionKey, outcome.migratedVersion);

    return outcome;
}

} // namespace

void
stampCurrentConfigSchemaVersion(YAML::Node &configRoot)
{
    stampSchemaVersion(configRoot,
                       SettingKey::ConfigSchemaVersion,
                       settings::schema_versions::kCurrentConfigSchemaVersion);
}

void
stampCurrentStateSchemaVersion(YAML::Node &stateRoot)
{
    stampSchemaVersion(stateRoot,
                       SettingKey::StateSchemaVersion,
                       settings::schema_versions::kCurrentStateSchemaVersion);
}

void
stampCurrentSessionSchemaVersion(YAML::Node &sessionRoot)
{
    stampSchemaVersion(sessionRoot,
                       SettingKey::SessionSchemaVersion,
                       settings::schema_versions::kCurrentSessionSchemaVersion);
}

ConfigMigrationOutcome
migrateConfigRoot(const YAML::Node &configRoot)
{
    return migrateRoot(configRoot,
                       SettingKey::ConfigSchemaVersion,
                       settings::schema_versions::kCurrentConfigSchemaVersion);
}

StateMigrationOutcome
migrateStateRoot(const YAML::Node &stateRoot)
{
    return migrateRoot(stateRoot,
                       SettingKey::StateSchemaVersion,
                       settings::schema_versions::kCurrentStateSchemaVersion);
}

SessionMigrationOutcome
migrateSessionRoot(const YAML::Node &sessionRoot)
{
    return migrateRoot(sessionRoot,
                       SettingKey::SessionSchemaVersion,
                       settings::schema_versions::kCurrentSessionSchemaVersion);
}

} // namespace settings::migrations
