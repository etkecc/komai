// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsController.h"
#include "SettingsControllerInternal.h"
#include "komai-rust-cxxbridge/ffi.h"

#include "logging/Logging.h"

#include "profile/Paths.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsSchemaVersions.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsSerializerLoad.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

using settings::storage::createDir;
using settings::storage::pathExists;

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
logConfigMigrationWarnings(const ::komai::rust::SettingsLoadedConfig &config)
{
    if (config.had_future_version) {
        settings::activeLoggers().ui->warn(
          "{} schema version {} is newer than supported version {}; "
          "known keys will still be loaded but no migration will be applied",
          "Config",
          config.source_version,
          settings::schema_versions::kCurrentConfigSchemaVersion);
    }
    if (config.had_unsupported_path) {
        settings::activeLoggers().ui->warn(
          "{} migration path is unsupported from schema version {} to {}; "
          "loaded values may be partially migrated",
          "Config",
          config.source_version,
          settings::schema_versions::kCurrentConfigSchemaVersion);
    }
}

void
logSessionMigrationWarnings(const ::komai::rust::SettingsLoadedSession &session)
{
    if (session.had_future_version) {
        settings::activeLoggers().ui->warn(
          "Session schema version {} is newer than supported version {}; "
          "known keys will still be loaded but no migration will be applied",
          session.source_version,
          settings::schema_versions::kCurrentSessionSchemaVersion);
    }
    if (session.had_unsupported_path) {
        settings::activeLoggers().ui->warn(
          "Session migration path is unsupported from schema version {} to {}; "
          "loaded values may be partially migrated",
          session.source_version,
          settings::schema_versions::kCurrentSessionSchemaVersion);
    }
}

void
logStateMigrationWarnings(const ::komai::rust::SettingsLoadedState &state)
{
    if (state.had_future_version) {
        settings::activeLoggers().ui->warn(
          "State schema version {} is newer than supported version {}; "
          "known keys will still be loaded but no migration will be applied",
          state.source_version,
          settings::schema_versions::kCurrentStateSchemaVersion);
    }
    if (state.had_unsupported_path) {
        settings::activeLoggers().ui->warn(
          "State migration path is unsupported from schema version {} to {}; "
          "loaded values may be partially migrated",
          state.source_version,
          settings::schema_versions::kCurrentStateSchemaVersion);
    }
}

void
setConfigStringValue(::rust::Vec<::komai::rust::SettingsConfigValue> &values,
                     const char *key,
                     const QString &value)
{
    for (auto &entry : values) {
        if (static_cast<std::string>(entry.key) != key)
            continue;

        entry.kind         = ::komai::rust::SettingsConfigValueKind::String;
        entry.bool_value   = false;
        entry.int_value    = 0;
        entry.double_value = 0.0;
        entry.string_value = value.toStdString();
        entry.string_list_value.clear();
        entry.string_list_map_value.clear();
        return;
    }

    values.push_back({.key                   = key,
                      .kind                  = ::komai::rust::SettingsConfigValueKind::String,
                      .bool_value            = false,
                      .int_value             = 0,
                      .double_value          = 0.0,
                      .string_value          = value.toStdString(),
                      .string_list_value     = {},
                      .string_list_map_value = {}});
}

bool
writeConfigSnapshot(const QString &path, const ::komai::rust::SettingsLoadedConfig &loaded)
{
    ::komai::rust::SettingsConfigSnapshot snapshot;
    snapshot.ui       = loaded.ui;
    snapshot.timeline = loaded.timeline;
    snapshot.secrets  = loaded.secrets;
    snapshot.privacy  = loaded.privacy;
    snapshot.calls    = loaded.calls;
    snapshot.values   = loaded.values;
    return ::komai::rust::settings_write_config_snapshot_to_path(path.toStdString(), snapshot);
}

void
loadImpl(UserSettings &settings,
         std::optional<QString> profile,
         bool persistMigrationWriteback,
         settings::SettingsController::LoadPolicy loadPolicy)
{
    if (profile)
        settings.applyProfilePathState((*profile == QLatin1String("default")) ? QLatin1String("")
                                                                              : *profile);
    else
        settings.applyProfilePathState(QLatin1String(""));

    createDir(settings.profileDirPath());
    const bool configFileExists  = pathExists(settings.configFilePath());
    const bool stateFileExists   = pathExists(settings.stateFilePath());
    const bool sessionFileExists = loadPolicy == settings::SettingsController::LoadPolicy::Full &&
                                   pathExists(settings.sessionFilePath());
    settings.setPersistenceSuspended(true);

    auto profileSnapshot = ::komai::rust::settings_load_profile_snapshot_from_paths(
      settings.configFilePath().toStdString(),
      settings.sessionFilePath().toStdString(),
      settings.stateFilePath().toStdString(),
      loadPolicy == settings::SettingsController::LoadPolicy::Full);
    auto &configSnapshot = profileSnapshot.config;
    logConfigMigrationWarnings(configSnapshot);
    settings::serializer::loadConfig(settings, configSnapshot);

    std::optional<bool> secureBackendAvailable;
    const auto secureBackendAvailableNow = [&]() -> bool {
        if (!secureBackendAvailable.has_value())
            secureBackendAvailable = settings::storage::isSecureBackendAvailable();
        return *secureBackendAvailable;
    };

    auto provider = settings::persistence::providerFromConfigValue(
      QString::fromStdString(static_cast<std::string>(configSnapshot.secrets.provider)));
    if (!configFileExists) {
        const bool secureAvailable = secureBackendAvailableNow();
        provider                   = preferredProviderForAvailability(secureAvailable);
        configSnapshot.secrets.provider =
          QString::fromLatin1(providerToken(provider)).toStdString();
        setConfigStringValue(configSnapshot.values,
                             SettingKey::SecretsProvider,
                             QString::fromLatin1(providerToken(provider)));
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

    if (persistMigrationWriteback && !configFileExists && !configSnapshot.had_future_version &&
        !configSnapshot.had_unsupported_path) {
        if (!writeConfigSnapshot(settings.configFilePath(), configSnapshot)) {
            settings::activeLoggers().ui->warn(
              "Failed to initialize new profile config with schema version: {}",
              settings.configFilePath().toStdString());
        } else {
            settings::activeLoggers().ui->info(
              "Initialized new profile config with settings schema version at: {}",
              settings.configFilePath().toStdString());
        }
    } else if (persistMigrationWriteback && configFileExists &&
               !configSnapshot.had_future_version && !configSnapshot.had_unsupported_path &&
               configSnapshot.should_write_back) {
        if (!::komai::rust::settings_write_loaded_config_to_path(
              settings.configFilePath().toStdString(), configSnapshot)) {
            settings::activeLoggers().ui->warn("Failed to persist migrated config settings at: {}",
                                               settings.configFilePath().toStdString());
        } else {
            settings::activeLoggers().ui->info(
              "Persisted config settings schema migration {} -> {} at: {}",
              configSnapshot.source_version,
              configSnapshot.migrated_version,
              settings.configFilePath().toStdString());
        }
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
            const auto &sessionSnapshot = profileSnapshot.session;
            logSessionMigrationWarnings(sessionSnapshot);
            settings.setSessionSnapshot(UserSettings::SessionSnapshot{
              .userId = QString::fromStdString(static_cast<std::string>(sessionSnapshot.user_id)),
              .accessToken = QString(),
              .deviceId =
                QString::fromStdString(static_cast<std::string>(sessionSnapshot.device_id)),
              .homeserver =
                QString::fromStdString(static_cast<std::string>(sessionSnapshot.homeserver))});
            if (persistMigrationWriteback && sessionFileExists &&
                !sessionSnapshot.had_future_version && !sessionSnapshot.had_unsupported_path &&
                sessionSnapshot.should_write_back) {
                if (!::komai::rust::settings_write_loaded_session_to_path(
                      settings.sessionFilePath().toStdString(), sessionSnapshot)) {
                    settings::activeLoggers().ui->warn(
                      "Failed to persist migrated session settings at: {}",
                      settings.sessionFilePath().toStdString());
                } else {
                    settings::activeLoggers().ui->info(
                      "Persisted session settings schema migration {} -> {} at: {}",
                      sessionSnapshot.source_version,
                      sessionSnapshot.migrated_version,
                      settings.sessionFilePath().toStdString());
                }
            }
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
            const auto &stateSnapshot = profileSnapshot.state;
            logStateMigrationWarnings(stateSnapshot);
            settings.setWindowWidth(stateSnapshot.window_width);
            settings.setWindowHeight(stateSnapshot.window_height);
            settings.setSidebarsRoomListWidthPx(stateSnapshot.sidebars_room_list_width_px);
            settings.setSidebarsCommunitiesWidthPx(stateSnapshot.sidebars_communities_width_px);
            settings.setCurrentFilterId(
              QString::fromStdString(static_cast<std::string>(stateSnapshot.current_filter_id)));
            settings.setCurrentRoomId(
              QString::fromStdString(static_cast<std::string>(stateSnapshot.current_room_id)));
            {
                QStringList values;
                for (const auto &value : stateSnapshot.global_excludes)
                    values.push_back(QString::fromStdString(static_cast<std::string>(value)));
                settings.setGlobalExcludes(values);
            }
            {
                QStringList values;
                for (const auto &value : stateSnapshot.badges_hidden_filters)
                    values.push_back(QString::fromStdString(static_cast<std::string>(value)));
                settings.setBadgesHiddenFilters(values);
            }
            {
                QStringList values;
                for (const auto &value : stateSnapshot.hidden_pins)
                    values.push_back(QString::fromStdString(static_cast<std::string>(value)));
                settings.setHiddenPins(values);
            }
            {
                QStringList values;
                for (const auto &value : stateSnapshot.hidden_widgets)
                    values.push_back(QString::fromStdString(static_cast<std::string>(value)));
                settings.setHiddenWidgets(values);
            }
            {
                QStringList values;
                for (const auto &value : stateSnapshot.collapsed_spaces)
                    values.push_back(QString::fromStdString(static_cast<std::string>(value)));
                settings.setCollapsedSpaces(values);
            }
            {
                QMap<QString, QString> drafts;
                for (const auto &entry : stateSnapshot.composer_drafts_by_room) {
                    drafts.insert(QString::fromStdString(static_cast<std::string>(entry.key)),
                                  QString::fromStdString(static_cast<std::string>(entry.value)));
                }
                settings.setComposerDraftsByRoom(drafts);
            }
            if (persistMigrationWriteback && stateFileExists && !stateSnapshot.had_future_version &&
                !stateSnapshot.had_unsupported_path && stateSnapshot.should_write_back) {
                if (!::komai::rust::settings_write_loaded_state_to_path(
                      settings.stateFilePath().toStdString(), stateSnapshot)) {
                    settings::activeLoggers().ui->warn(
                      "Failed to persist migrated state settings at: {}",
                      settings.stateFilePath().toStdString());
                } else {
                    settings::activeLoggers().ui->info(
                      "Persisted state settings schema migration {} -> {} at: {}",
                      stateSnapshot.source_version,
                      stateSnapshot.migrated_version,
                      settings.stateFilePath().toStdString());
                }
            }
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
                configSnapshot.secrets.provider =
                  QString::fromLatin1(providerToken(provider)).toStdString();
                setConfigStringValue(configSnapshot.values,
                                     SettingKey::SecretsProvider,
                                     QString::fromLatin1(providerToken(provider)));

                if (persistMigrationWriteback && !configSnapshot.had_future_version &&
                    !configSnapshot.had_unsupported_path) {
                    if (!writeConfigSnapshot(settings.configFilePath(), configSnapshot)) {
                        settings::activeLoggers().ui->warn(
                          "Failed to persist startup secrets provider update at: {}",
                          settings.configFilePath().toStdString());
                    } else {
                        settings::activeLoggers().ui->info(
                          "Persisted startup secrets provider update at: {}",
                          settings.configFilePath().toStdString());
                    }
                } else if (persistMigrationWriteback && (configSnapshot.had_future_version ||
                                                         configSnapshot.had_unsupported_path)) {
                    settings::activeLoggers().ui->warn(
                      "Skipped persisting startup secrets provider update for profile '{}' "
                      "(future_version={}, unsupported_path={})",
                      app_paths::normalizedProfileId(settings.profileId()).toStdString(),
                      configSnapshot.had_future_version ? "true" : "false",
                      configSnapshot.had_unsupported_path ? "true" : "false");
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
    loadImpl(settings, profile, false, policy);
}

void
settings::SettingsController::loadAndMigrate(UserSettings &settings,
                                             std::optional<QString> profile,
                                             LoadPolicy policy)
{
    loadImpl(settings, profile, true, policy);
}
