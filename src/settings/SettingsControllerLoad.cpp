// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsController.h"
#include "SettingsControllerInternal.h"

#include <spdlog/logger.h>
#include <yaml-cpp/yaml.h>

#include "profile/Paths.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsMigrations.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/YamlSettings.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

using settings::storage::createDir;
using settings::storage::loadYamlFile;
using settings::storage::pathExists;
using settings::storage::writeYamlFile;

const char *
providerToken(staged_load_plan::SecretsProvider provider)
{
    return provider == staged_load_plan::SecretsProvider::File
             ? staged_load_plan::ProviderFileValue
             : staged_load_plan::ProviderSecretServiceValue;
}

staged_load_plan::SecretsProvider
preferredProviderForAvailability(bool secureBackendAvailable)
{
    return secureBackendAvailable ? staged_load_plan::SecretsProvider::SecretService
                                  : staged_load_plan::SecretsProvider::File;
}

void
setConfigSecretsProvider(YAML::Node &configRoot, staged_load_plan::SecretsProvider provider)
{
    yaml_settings::setNode(configRoot,
                           SettingKey::SecretsProvider,
                           QString::fromLatin1(providerToken(provider)).toStdString());
}

void
logMigrationWarnings(const char *scopeName,
                     const settings::migrations::ScopeMigrationOutcome &outcome,
                     int currentVersion)
{
    if (outcome.hadFutureVersion) {
        settings::activeLoggers().ui->warn(
          "{} schema version {} is newer than supported version {}; "
          "known keys will still be loaded but no migration will be applied",
          scopeName,
          outcome.sourceVersion,
          currentVersion);
    }
    if (outcome.hadUnsupportedPath) {
        settings::activeLoggers().ui->warn(
          "{} migration path is unsupported from schema version {} to {}; "
          "loaded values may be partially migrated",
          scopeName,
          outcome.sourceVersion,
          currentVersion);
    }
}

void
writeMigratedScopeIfNeeded(const QString &path,
                           const char *scopeName,
                           bool persistMigrationWriteback,
                           bool fileExists,
                           const settings::migrations::ScopeMigrationOutcome &outcome)
{
    if (!persistMigrationWriteback || !fileExists || outcome.hadFutureVersion ||
        outcome.hadUnsupportedPath || outcome.sourceVersion == outcome.migratedVersion)
        return;

    if (!writeYamlFile(path, outcome.migratedRoot, false)) {
        settings::activeLoggers().ui->warn(
          "Failed to persist migrated {} settings at: {}", scopeName, path.toStdString());
        return;
    }

    settings::activeLoggers().ui->info("Persisted {} settings schema migration {} -> {} at: {}",
                                       scopeName,
                                       outcome.sourceVersion,
                                       outcome.migratedVersion,
                                       path.toStdString());
}

void
loadImpl(UserSettings &settings,
         std::optional<QString> profile,
         const YAML::Node &configRoot,
         bool persistMigrationWriteback,
         settings::SettingsController::LoadPolicy loadPolicy)
{
    if (profile)
        settings.applyProfilePathState((*profile == QLatin1String("default")) ? QLatin1String("")
                                                                              : *profile);
    else
        settings.applyProfilePathState(QLatin1String(""));

    createDir(settings.profileDirPath());
    const bool configFileExists = pathExists(settings.configFilePath());
    settings.setPersistenceSuspended(true);

    const bool hasInjectedConfig = configRoot.IsDefined() && !configRoot.IsNull();
    const auto loadedConfig =
      hasInjectedConfig ? configRoot : loadYamlFile(settings.configFilePath(), "config");
    auto configOutcome = settings::migrations::migrateConfigRoot(loadedConfig);
    logMigrationWarnings(
      "Config", configOutcome, settings::migrations::kCurrentConfigSchemaVersion);
    settings::serializer::loadConfig(settings, configOutcome.migratedRoot);

    std::optional<bool> secureBackendAvailable;
    const auto secureBackendAvailableNow = [&]() -> bool {
        if (!secureBackendAvailable.has_value())
            secureBackendAvailable = settings::storage::isSecureBackendAvailable();
        return *secureBackendAvailable;
    };

    auto provider = settings::persistence::providerFromConfig(configOutcome.migratedRoot);
    if (!configFileExists) {
        const bool secureAvailable = secureBackendAvailableNow();
        provider                   = preferredProviderForAvailability(secureAvailable);
        setConfigSecretsProvider(configOutcome.migratedRoot, provider);
        settings::activeLoggers().ui->info(
          "New profile '{}' selected secrets provider '{}' (secure backend available={})",
          app_paths::normalizedProfileId(settings.profileId()).toStdString(),
          providerToken(provider),
          secureAvailable ? "true" : "false");
    } else {
        settings::activeLoggers().ui->debug(
          "Profile '{}' configured secrets provider '{}'",
          app_paths::normalizedProfileId(settings.profileId()).toStdString(),
          providerToken(provider));
    }

    if (persistMigrationWriteback && !configFileExists && !configOutcome.hadFutureVersion &&
        !configOutcome.hadUnsupportedPath) {
        if (!writeYamlFile(settings.configFilePath(), configOutcome.migratedRoot, false)) {
            settings::activeLoggers().ui->warn(
              "Failed to initialize new profile config with schema version: {}",
              settings.configFilePath().toStdString());
        } else {
            settings::activeLoggers().ui->info(
              "Initialized new profile config with settings schema version at: {}",
              settings.configFilePath().toStdString());
        }
    } else {
        writeMigratedScopeIfNeeded(settings.configFilePath(),
                                   "config",
                                   persistMigrationWriteback,
                                   configFileExists,
                                   configOutcome);
    }

    settings.setUsesFileSecretsProvider(provider == staged_load_plan::SecretsProvider::File);

    const auto stages = loadPolicy == settings::SettingsController::LoadPolicy::ConfigAndStateOnly
                          ? QList<staged_load_plan::Stage>{staged_load_plan::Stage::Config,
                                                           staged_load_plan::Stage::State}
                          : staged_load_plan::stagesForProvider(provider);
    for (const auto stage : stages) {
        switch (stage) {
        case staged_load_plan::Stage::Config:
            break;
        case staged_load_plan::Stage::Session: {
            const bool sessionFileExists = pathExists(settings.sessionFilePath());
            const auto loadedSession     = loadYamlFile(settings.sessionFilePath(), "session");
            const auto sessionOutcome    = settings::migrations::migrateSessionRoot(loadedSession);
            logMigrationWarnings(
              "Session", sessionOutcome, settings::migrations::kCurrentSessionSchemaVersion);
            settings::serializer::loadSession(settings, sessionOutcome.migratedRoot);
            writeMigratedScopeIfNeeded(settings.sessionFilePath(),
                                       "session",
                                       persistMigrationWriteback,
                                       sessionFileExists,
                                       sessionOutcome);
            break;
        }
        case staged_load_plan::Stage::SecretsSecureBackend:
        case staged_load_plan::Stage::SecretsFile: {
            const auto payload = settings::persistence::loadProfileSecrets(
              settings.profileId(), settings.usesFileSecretsProvider(), settings.secretsFilePath());
            settings.applyLoadedSecrets(payload.accessToken, payload.secrets);
            break;
        }
        case staged_load_plan::Stage::State: {
            const bool stateFileExists = pathExists(settings.stateFilePath());
            const auto loadedState     = loadYamlFile(settings.stateFilePath(), "state");
            const auto stateOutcome    = settings::migrations::migrateStateRoot(loadedState);
            logMigrationWarnings(
              "State", stateOutcome, settings::migrations::kCurrentStateSchemaVersion);
            settings::serializer::loadState(settings, stateOutcome.migratedRoot);
            writeMigratedScopeIfNeeded(settings.stateFilePath(),
                                       "state",
                                       persistMigrationWriteback,
                                       stateFileExists,
                                       stateOutcome);
            break;
        }
        }
    }

    if (loadPolicy == settings::SettingsController::LoadPolicy::Full) {
        const bool hasActiveSession            = settings.hasActiveSession();
        const bool hasPersistedSessionIdentity = settings.hasPersistedSessionIdentity();
        if (!hasActiveSession && !hasPersistedSessionIdentity) {
            const bool secureAvailable   = secureBackendAvailableNow();
            const auto preferredProvider = preferredProviderForAvailability(secureAvailable);
            if (preferredProvider != provider) {
                settings::activeLoggers().ui->info(
                  "Profile '{}' has no active session; switching secrets provider '{}' -> '{}' "
                  "(secure backend available={})",
                  app_paths::normalizedProfileId(settings.profileId()).toStdString(),
                  providerToken(provider),
                  providerToken(preferredProvider),
                  secureAvailable ? "true" : "false");
                provider = preferredProvider;
                settings.setUsesFileSecretsProvider(provider ==
                                                    staged_load_plan::SecretsProvider::File);
                setConfigSecretsProvider(configOutcome.migratedRoot, provider);

                if (persistMigrationWriteback && !configOutcome.hadFutureVersion &&
                    !configOutcome.hadUnsupportedPath) {
                    if (!writeYamlFile(
                          settings.configFilePath(), configOutcome.migratedRoot, false)) {
                        settings::activeLoggers().ui->warn(
                          "Failed to persist startup secrets provider update at: {}",
                          settings.configFilePath().toStdString());
                    } else {
                        settings::activeLoggers().ui->info(
                          "Persisted startup secrets provider update at: {}",
                          settings.configFilePath().toStdString());
                    }
                } else if (persistMigrationWriteback &&
                           (configOutcome.hadFutureVersion || configOutcome.hadUnsupportedPath)) {
                    settings::activeLoggers().ui->warn(
                      "Skipped persisting startup secrets provider update for profile '{}' "
                      "(future_version={}, unsupported_path={})",
                      app_paths::normalizedProfileId(settings.profileId()).toStdString(),
                      configOutcome.hadFutureVersion ? "true" : "false",
                      configOutcome.hadUnsupportedPath ? "true" : "false");
                }
            } else {
                settings::activeLoggers().ui->debug(
                  "Profile '{}' has no active session; secrets provider '{}' unchanged "
                  "(secure backend available={})",
                  app_paths::normalizedProfileId(settings.profileId()).toStdString(),
                  providerToken(provider),
                  secureAvailable ? "true" : "false");
            }
            settings.setSecretsProviderFallbackWarningVisible(
              provider == staged_load_plan::SecretsProvider::File && !secureAvailable);
        } else {
            settings.setSecretsProviderFallbackWarningVisible(false);
            if (!hasActiveSession && hasPersistedSessionIdentity) {
                settings::activeLoggers().ui->warn(
                  "Profile '{}' has persisted session identity but no active session auth; "
                  "skipping startup secrets-provider auto-switch to avoid credential loss",
                  app_paths::normalizedProfileId(settings.profileId()).toStdString());
            }
        }
    } else {
        settings.setSecretsProviderFallbackWarningVisible(false);
    }

    settings.applyTheme();
    // Keep the core store synchronized from validated runtime settings only.
    // Avoid re-importing raw YAML scalars here, because that can bypass token
    // conversion/validation paths and reintroduce invalid persisted values.
    settings::syncCoreStoreFromSettings(settings);
    settings.setPersistenceScopeReadyForAuth(settings.hasActiveSession());
    // Keep persistence intentionally paused until the UI startup sequence completes.
    // This avoids incidental `save()` calls from initialization code paths.
    settings.setPersistenceSuspended(true);

    if (profile)
        settings.notifyProfileChanged();
}

} // namespace

void
settings::SettingsController::load(UserSettings &settings,
                                   std::optional<QString> profile,
                                   LoadPolicy policy)
{
    load(settings, profile, YAML::Node{}, policy);
}

void
settings::SettingsController::load(UserSettings &settings,
                                   std::optional<QString> profile,
                                   const YAML::Node &configRoot,
                                   LoadPolicy policy)
{
    loadImpl(settings, profile, configRoot, false, policy);
}

void
settings::SettingsController::loadAndMigrate(UserSettings &settings,
                                             std::optional<QString> profile,
                                             LoadPolicy policy)
{
    loadAndMigrate(settings, profile, YAML::Node{}, policy);
}

void
settings::SettingsController::loadAndMigrate(UserSettings &settings,
                                             std::optional<QString> profile,
                                             const YAML::Node &configRoot,
                                             LoadPolicy policy)
{
    loadImpl(settings, profile, configRoot, true, policy);
}
