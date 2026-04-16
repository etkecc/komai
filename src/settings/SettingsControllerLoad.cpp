// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsController.h"
#include "SettingsControllerInternal.h"
#include "komai-rust-cxxbridge/ffi.h"

#include <QSet>

#include <utility>

#include "logging/Logging.h"

#include "profile/Paths.h"
#include "settings/SettingsSchemaVersions.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsSerializerLoad.h"
#include "settings/SettingsStorage.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

QMap<QString, QString>
fromRustStringMapEntries(const ::rust::Vec<::komai::rust::SettingsStringMapEntry> &entries)
{
    QMap<QString, QString> secrets;
    for (const auto &entry : entries) {
        secrets[QString::fromStdString(static_cast<std::string>(entry.key))] =
          QString::fromStdString(static_cast<std::string>(entry.value));
    }
    return secrets;
}

void
logLoadedSecrets(QStringView profileId,
                 bool usesFileSecretsProvider,
                 const ::komai::rust::SettingsSecretsPayload &payload)
{
    if (usesFileSecretsProvider) {
        settings::activeLoggers().ui->info(
          "Loaded file-backed secrets (has_access_token={}, secrets_count={})",
          !QString::fromStdString(static_cast<std::string>(payload.access_token))
             .trimmed()
             .isEmpty(),
          payload.secrets.size());
        return;
    }

    if (payload.had_stale_values) {
        settings::activeLoggers().ui->warn(
          "Found stale/empty secure backend values for profile '{}'",
          app_paths::normalizedProfileId(profileId).toStdString());
    }

    settings::activeLoggers().ui->info(
      "Loaded secure-backend secrets (has_access_token={}, secrets_count={})",
      !QString::fromStdString(static_cast<std::string>(payload.access_token)).trimmed().isEmpty(),
      payload.secrets.size());
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

struct NormalizedTabRestoreState
{
    QStringList openTabs;
    QStringList pinnedTabs;
    QString currentRoomId;
    bool changed = false;
};

NormalizedTabRestoreState
normalizeTabRestoreState(const QStringList &loadedOpenTabs,
                         const QStringList &loadedPinnedTabs,
                         const QString &loadedCurrentRoomId)
{
    NormalizedTabRestoreState normalized{
      .openTabs      = {},
      .pinnedTabs    = {},
      .currentRoomId = loadedCurrentRoomId.trimmed(),
      .changed       = false,
    };

    QSet<QString> seenOpenTabs;
    for (const auto &roomId : loadedOpenTabs) {
        const auto normalizedRoomId = roomId.trimmed();
        if (seenOpenTabs.contains(normalizedRoomId))
            continue;

        seenOpenTabs.insert(normalizedRoomId);
        normalized.openTabs.push_back(normalizedRoomId);
    }

    if (normalized.openTabs.isEmpty()) {
        normalized.openTabs.push_back(
          normalized.currentRoomId.isEmpty() ? QString() : normalized.currentRoomId);
    } else if (normalized.currentRoomId.isEmpty()) {
        if (!normalized.openTabs.contains(QString()))
            normalized.currentRoomId = normalized.openTabs.front();
    } else if (!normalized.openTabs.contains(normalized.currentRoomId)) {
        normalized.openTabs.push_back(normalized.currentRoomId);
    }

    QSet<QString> openTabsSet;
    for (const auto &roomId : normalized.openTabs)
        openTabsSet.insert(roomId);

    QSet<QString> seenPinnedTabs;
    for (const auto &roomId : loadedPinnedTabs) {
        const auto normalizedRoomId = roomId.trimmed();
        if (normalizedRoomId.isEmpty() || !openTabsSet.contains(normalizedRoomId) ||
            seenPinnedTabs.contains(normalizedRoomId)) {
            continue;
        }

        seenPinnedTabs.insert(normalizedRoomId);
        normalized.pinnedTabs.push_back(normalizedRoomId);
    }

    normalized.changed = normalized.openTabs != loadedOpenTabs ||
                         normalized.pinnedTabs != loadedPinnedTabs ||
                         normalized.currentRoomId != loadedCurrentRoomId;
    return normalized;
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

    settings.setPersistenceSuspended(true);

    const bool secureBackendAvailable = settings::storage::isSecureBackendAvailable();
    auto profileHandle                = ::komai::rust::settings_open_profile_handle_for_profile(
      settings.profileId().toStdString(),
      loadPolicy == settings::SettingsController::LoadPolicy::Full);
    ::komai::rust::settings_profile_prepare_for_load(
      *profileHandle,
      loadPolicy == settings::SettingsController::LoadPolicy::Full,
      secureBackendAvailable);
    auto profileSnapshot              = ::komai::rust::settings_profile_snapshot(*profileHandle);
    auto &configSnapshot              = profileSnapshot.config;
    const bool configFileExists       = configSnapshot.source_exists;
    const bool configShouldInitialize = !configFileExists;
    const bool configShouldWriteBack  = configSnapshot.should_write_back;
    const bool startupSecretsProviderChanged = profileSnapshot.startup_secrets_provider_changed;
    logConfigMigrationWarnings(configSnapshot);
    settings::serializer::loadConfig(settings, configSnapshot);
    if (!configFileExists) {
        settings::activeLoggers().ui->info(
          "New profile '{}' selected secrets provider '{}' (secure backend available={})",
          app_paths::normalizedProfileId(settings.profileId()).toStdString(),
          static_cast<std::string>(configSnapshot.secrets.provider),
          secureBackendAvailable ? "true" : "false");
    } else {
        settings::activeLoggers().ui->debug(
          "Profile '{}' configured secrets provider '{}'",
          app_paths::normalizedProfileId(settings.profileId()).toStdString(),
          static_cast<std::string>(configSnapshot.secrets.provider));
    }

    settings.setUsesFileSecretsProvider(profileSnapshot.uses_file_secrets_provider);

    if (loadPolicy == settings::SettingsController::LoadPolicy::Full) {
        const auto &sessionSnapshot  = profileSnapshot.session;
        const bool sessionFileExists = sessionSnapshot.source_exists;
        logSessionMigrationWarnings(sessionSnapshot);
        settings.setSessionSnapshot(UserSettings::SessionSnapshot{
          .userId      = QString::fromStdString(static_cast<std::string>(sessionSnapshot.user_id)),
          .accessToken = QString(),
          .deviceId = QString::fromStdString(static_cast<std::string>(sessionSnapshot.device_id)),
          .homeserver =
            QString::fromStdString(static_cast<std::string>(sessionSnapshot.homeserver))});
        if (persistMigrationWriteback && sessionFileExists && !sessionSnapshot.had_future_version &&
            !sessionSnapshot.had_unsupported_path && sessionSnapshot.should_write_back) {
            const auto result =
              ::komai::rust::settings_profile_flush(*profileHandle, false, true, false, false);
            if (!result.session_saved) {
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

        const auto &payload = profileSnapshot.secrets;
        settings.applyLoadedSecrets(
          QString::fromStdString(static_cast<std::string>(payload.access_token)),
          fromRustStringMapEntries(payload.secrets));
        logLoadedSecrets(settings.profileId(), settings.usesFileSecretsProvider(), payload);
    }

    {
        const auto &stateSnapshot  = profileSnapshot.state;
        const bool stateFileExists = stateSnapshot.source_exists;
        logStateMigrationWarnings(stateSnapshot);
        settings.setSponsoringStatus(
          QString::fromStdString(static_cast<std::string>(stateSnapshot.sponsoring_status)));
        settings.setWindowWidth(stateSnapshot.window_width);
        settings.setWindowHeight(stateSnapshot.window_height);
        settings.setNavigationRoomListWidthPx(stateSnapshot.navigation_room_list_width_px);
        settings.setNavigationCommunitiesWidthPx(stateSnapshot.navigation_communities_width_px);
        QString currentRoomId =
          QString::fromStdString(static_cast<std::string>(stateSnapshot.current_room_id));
        settings.setCurrentFilterId(
          QString::fromStdString(static_cast<std::string>(stateSnapshot.current_filter_id)));
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
            QStringList values;
            for (const auto &value : stateSnapshot.hidden_spaces)
                values.push_back(QString::fromStdString(static_cast<std::string>(value)));
            settings.setHiddenSpaces(values);
        }
        QStringList openTabs;
        {
            for (const auto &value : stateSnapshot.open_tabs)
                openTabs.push_back(QString::fromStdString(static_cast<std::string>(value)));
        }
        QStringList pinnedTabs;
        {
            for (const auto &value : stateSnapshot.pinned_tabs)
                pinnedTabs.push_back(QString::fromStdString(static_cast<std::string>(value)));
        }
        const auto normalizedTabs = normalizeTabRestoreState(openTabs, pinnedTabs, currentRoomId);
        if (normalizedTabs.changed) {
            settings::activeLoggers().ui->info(
              "Normalized persisted tab restore state for profile '{}' "
              "(open_tabs={} -> {}, pinned_tabs={} -> {}, current_room_id='{}' -> '{}')",
              app_paths::normalizedProfileId(settings.profileId()).toStdString(),
              openTabs.size(),
              normalizedTabs.openTabs.size(),
              pinnedTabs.size(),
              normalizedTabs.pinnedTabs.size(),
              currentRoomId.toStdString(),
              normalizedTabs.currentRoomId.toStdString());
        }
        settings.setCurrentRoomId(normalizedTabs.currentRoomId);
        settings.setOpenTabs(normalizedTabs.openTabs);
        settings.setPinnedTabs(normalizedTabs.pinnedTabs);
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
            const auto result =
              ::komai::rust::settings_profile_flush(*profileHandle, false, false, false, true);
            if (!result.state_saved) {
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
    }

    if (loadPolicy == settings::SettingsController::LoadPolicy::Full) {
        const bool hasActiveSession            = settings.hasActiveSession();
        const bool hasPersistedSessionIdentity = settings.hasPersistedSessionIdentity();
        settings.setSecretsProviderFallbackWarningVisible(
          profileSnapshot.secrets_provider_fallback_warning_visible);
        if (!hasActiveSession && hasPersistedSessionIdentity) {
            settings::activeLoggers().ui->warn(
              "Profile '{}' has persisted session identity but no active session auth; "
              "skipping startup secrets-provider auto-switch to avoid credential loss",
              app_paths::normalizedProfileId(settings.profileId()).toStdString());
        }
    } else {
        settings.setSecretsProviderFallbackWarningVisible(false);
    }

    if (startupSecretsProviderChanged && persistMigrationWriteback &&
        (configSnapshot.had_future_version || configSnapshot.had_unsupported_path)) {
        settings::activeLoggers().ui->warn(
          "Skipped persisting startup secrets provider update for profile '{}' "
          "(future_version={}, unsupported_path={})",
          app_paths::normalizedProfileId(settings.profileId()).toStdString(),
          configSnapshot.had_future_version ? "true" : "false",
          configSnapshot.had_unsupported_path ? "true" : "false");
    }

    if (persistMigrationWriteback && !configSnapshot.had_future_version &&
        !configSnapshot.had_unsupported_path) {
        const auto result =
          ::komai::rust::settings_profile_flush(*profileHandle, true, false, false, false);
        if (result.config_saved) {
            if (startupSecretsProviderChanged) {
                settings::activeLoggers().ui->info(
                  "Persisted startup secrets provider update at: {}",
                  settings.configFilePath().toStdString());
            } else if (configShouldInitialize) {
                settings::activeLoggers().ui->info(
                  "Initialized new profile config with settings schema version at: {}",
                  settings.configFilePath().toStdString());
            } else if (configShouldWriteBack) {
                settings::activeLoggers().ui->info(
                  "Persisted config settings schema migration {} -> {} at: {}",
                  configSnapshot.source_version,
                  configSnapshot.migrated_version,
                  settings.configFilePath().toStdString());
            }
        } else if (result.config_attempted) {
            if (startupSecretsProviderChanged) {
                settings::activeLoggers().ui->warn(
                  "Failed to persist startup secrets provider update at: {}",
                  settings.configFilePath().toStdString());
            } else if (configShouldInitialize) {
                settings::activeLoggers().ui->warn(
                  "Failed to initialize new profile config with schema version: {}",
                  settings.configFilePath().toStdString());
            } else if (configShouldWriteBack) {
                settings::activeLoggers().ui->warn(
                  "Failed to persist migrated config settings at: {}",
                  settings.configFilePath().toStdString());
            }
        }
    }

    settings.setRustSettingsProfileHandle(std::move(profileHandle));
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
