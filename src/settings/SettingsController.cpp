// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <yaml-cpp/yaml.h>

#include "SettingsController.h"
#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>
#include <utility>

#include "Paths.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"
#include "settings/core/SettingDefinition.h"
#include "settings/core/SettingsConstraints.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/core/SettingsSerializer.h"
#include "settings/ui/facade/UserSettingsCoreStoreBridge.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include <string>
#include <string_view>

namespace {

using settings::storage::createDir;
using settings::storage::loadYamlFile;
using settings::storage::pathExists;
using settings::storage::removePath;
using settings::storage::writeYamlFile;

using settings::persistence::providerFromConfig;

} // namespace

namespace {

std::shared_ptr<spdlog::logger>
nullLogger(std::string_view name)
{
    static auto sink   = std::make_shared<spdlog::sinks::null_sink_mt>();
    static auto logger = std::make_shared<spdlog::logger>(std::string(name), sink);
    return logger;
}

settings::ControllerLoggers
defaultLoggers()
{
    return {.ui = nullLogger("settings-controller-ui")};
}

settings::ControllerLoggers &
currentLoggers()
{
    static settings::ControllerLoggers loggers = defaultLoggers();
    return loggers;
}

void
syncCoreStoreFromSettings(UserSettings &settings)
{
    auto &store = settings.mutableCoreStore();
    store.clear();
    settings::core::constraints::applyDefaultConstraints(store);

    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        const auto value =
          settings::ui::facade::coreStoreValueForSettingId(settings, definition.id);
        if (!value.has_value()) {
            currentLoggers().ui->warn("No core-store mapping for setting id {}",
                                      static_cast<int>(definition.id));
            continue;
        }

        const auto result = store.setValue(definition.id, *value);
        if (!result.success) {
            currentLoggers().ui->warn("Invalid value for setting id {} ignored: {}",
                                      static_cast<int>(definition.id),
                                      result.validationError);
        }
    }
}

const YAML::Node *
rootNodeForScope(settings::core::SettingScope scope,
                 const YAML::Node &configRoot,
                 const YAML::Node &stateRoot,
                 const YAML::Node &sessionRoot)
{
    switch (scope) {
    case settings::core::SettingScope::Config:
        return &configRoot;
    case settings::core::SettingScope::State:
        return &stateRoot;
    case settings::core::SettingScope::Session:
        return &sessionRoot;
    case settings::core::SettingScope::Runtime:
    case settings::core::SettingScope::Secrets:
        return nullptr;
    }

    return nullptr;
}

void
syncCoreStoreFromPersistence(UserSettings &settings,
                             const YAML::Node &configRoot,
                             const YAML::Node &stateRoot,
                             const YAML::Node &sessionRoot)
{
    syncCoreStoreFromSettings(settings);

    auto &store = settings.mutableCoreStore();
    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.id == settings::core::SettingId::Unknown || !definition.persistedKey)
            continue;

        const auto *root = rootNodeForScope(definition.scope, configRoot, stateRoot, sessionRoot);
        if (!root)
            continue;

        const auto defaultValue = store.value(definition.id);
        if (!defaultValue.has_value())
            continue;

        const auto node = yaml_settings::getNode(*root, definition.persistedKey);
        (void)settings::core::serializer::setFromYamlNodeOrDefault(
          store, definition.id, node, *defaultValue);
    }
}

void
syncConfigYamlFromCoreStore(const QString &configFilePath,
                            const settings::core::SettingsStore &store)
{
    YAML::Node configRoot = loadYamlFile(configFilePath, "config");
    bool changed          = false;

    for (const auto &definition : settings::core::definitions::persistedDefinitions()) {
        if (definition.id == settings::core::SettingId::Unknown || !definition.persistedKey ||
            definition.scope != settings::core::SettingScope::Config)
            continue;

        const auto value = store.value(definition.id);
        if (!value.has_value())
            continue;

        yaml_settings::setNode(
          configRoot, definition.persistedKey, settings::core::serializer::toYamlNode(*value));
        changed = true;
    }

    if (changed)
        writeYamlFile(configFilePath, configRoot, true);
}

} // namespace

void
settings::setLoggers(settings::ControllerLoggers loggers)
{
    const auto &defaults = defaultLoggers();
    if (!loggers.ui)
        loggers.ui = defaults.ui;
    currentLoggers() = std::move(loggers);
}

const settings::ControllerLoggers &
settings::activeLoggers()
{
    return currentLoggers();
}

void
settings::SettingsController::load(UserSettings &settings, std::optional<QString> profile)
{
    load(settings, profile, YAML::Node());
}

void
settings::SettingsController::load(UserSettings &settings,
                                   std::optional<QString> profile,
                                   const YAML::Node &configRoot)
{
    if (profile)
        settings.applyProfilePathState((*profile == QLatin1String("default")) ? QLatin1String("")
                                                                              : *profile);
    else
        settings.applyProfilePathState(QLatin1String(""));

    createDir(settings.profileDirPath());

    settings.setPersistenceSuspended(true);

    const auto effectiveConfig =
      configRoot.IsDefined() ? configRoot : loadYamlFile(settings.configFilePath(), "config");
    settings::serializer::loadConfig(settings, effectiveConfig);

    const auto provider = providerFromConfig(effectiveConfig);
    settings.setUsesFileSecretsProvider(provider == staged_load_plan::SecretsProvider::File);
    YAML::Node sessionRoot;
    YAML::Node stateRoot;

    for (const auto stage : staged_load_plan::stagesForProvider(provider)) {
        switch (stage) {
        case staged_load_plan::Stage::Config:
            break;
        case staged_load_plan::Stage::Session: {
            sessionRoot = loadYamlFile(settings.sessionFilePath(), "session");
            settings::serializer::loadSession(settings, sessionRoot);
            break;
        }
        case staged_load_plan::Stage::SecretsSecureBackend:
        case staged_load_plan::Stage::SecretsFile: {
            const auto payload = settings::persistence::loadProfileSecrets(
              settings.profileId(), settings.usesFileSecretsProvider(), settings.secretsFilePath());
            settings.applyLoadedSecrets(payload.accessToken, payload.secrets);

            const auto snapshot = settings.sessionSnapshot();
            if ((snapshot.userId.isEmpty() || snapshot.deviceId.isEmpty() ||
                 snapshot.homeserver.isEmpty()) &&
                !payload.sessionUserId.isEmpty() && !payload.sessionDeviceId.isEmpty() &&
                !payload.sessionHomeserver.isEmpty()) {
                settings.setSessionSnapshot(
                  UserSettings::SessionSnapshot{.userId      = payload.sessionUserId,
                                                .accessToken = payload.accessToken,
                                                .deviceId    = payload.sessionDeviceId,
                                                .homeserver  = payload.sessionHomeserver});
            }
            break;
        }
        case staged_load_plan::Stage::State: {
            stateRoot = loadYamlFile(settings.stateFilePath(), "state");
            settings::serializer::loadState(settings, stateRoot);
            break;
        }
        }
    }

    settings.applyTheme();
    syncCoreStoreFromPersistence(settings, effectiveConfig, stateRoot, sessionRoot);
    settings.setPersistenceScopeReadyForAuth(settings.hasActiveSession());
    // Keep persistence intentionally paused until the UI startup sequence completes.
    // This avoids incidental `save()` calls from initialization code paths.
    settings.setPersistenceSuspended(true);

    if (profile)
        emit settings.profileChanged(settings.profileId());
}

void
settings::SettingsController::save(UserSettings &settings, SavePolicy policy)
{
    if (!settings.hasResolvedProfilePaths()) {
        settings.applyProfilePathState(settings.profileId());
        createDir(settings.profileDirPath());
    }

    syncCoreStoreFromSettings(settings);

    settings::serializer::saveConfig(settings, settings.configFilePath());
    syncConfigYamlFromCoreStore(settings.configFilePath(), settings.coreStore());

    if (policy == SavePolicy::Full) {
        settings::serializer::saveSession(settings, settings.sessionFilePath());

        const auto provider = settings::persistence::providerFromConfig(
          settings::storage::loadYamlFile(settings.configFilePath(), "config"));
        settings::persistence::saveProfileSecrets(settings.profileId(),
                                                  provider ==
                                                    staged_load_plan::SecretsProvider::File,
                                                  settings.secretsFilePath(),
                                                  settings.accessToken(),
                                                  settings.secretsMap(),
                                                  settings.userId(),
                                                  settings.deviceId(),
                                                  settings.homeserver());

        settings::serializer::saveState(settings, settings.stateFilePath());
    }
    syncCoreStoreFromSettings(settings);
}

void
settings::SettingsController::clearAuth(UserSettings &settings)
{
    activeLoggers().ui->info("Clearing persisted session auth/identity for profile '{}'",
                             app_paths::normalizedProfileId(settings.profileId()).toStdString());

    settings.clearAuthInMemory();

    if (pathExists(settings.sessionFilePath()) && !removePath(settings.sessionFilePath())) {
        activeLoggers().ui->warn("Failed to remove session file '{}', keeping file to avoid "
                                 "accidental data loss",
                                 settings.sessionFilePath().toStdString());
    }

    const auto provider = providerFromConfig(loadYamlFile(settings.configFilePath(), "config"));
    settings::persistence::clearProfileSecrets(settings.profileId(),
                                               provider == staged_load_plan::SecretsProvider::File,
                                               settings.secretsFilePath());
    settings::serializer::saveState(settings, settings.stateFilePath());
    syncCoreStoreFromSettings(settings);
}
