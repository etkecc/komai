// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <yaml-cpp/yaml.h>

#include "SettingsController.h"
#include <spdlog/logger.h>
#include <spdlog/sinks/null_sink.h>
#include <utility>

#include <string>
#include <string_view>

#include "Paths.h"
#include "UserSettingsPage.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"

namespace {

using settings::storage::configFilePathForProfile;
using settings::storage::createDir;
using settings::storage::loadYamlFile;
using settings::storage::pathExists;
using settings::storage::profileDirPath;
using settings::storage::removePath;
using settings::storage::secretsFilePathForProfile;
using settings::storage::sessionFilePathForProfile;
using settings::storage::stateFilePathForProfile;

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
        settings.profile_ = (*profile == QLatin1String("default")) ? QLatin1String("") : *profile;
    else
        settings.profile_ = QLatin1String("");

    settings.profileDirPath_  = profileDirPath(settings.profile_);
    settings.configFilePath_  = configFilePathForProfile(settings.profile_);
    settings.stateFilePath_   = stateFilePathForProfile(settings.profile_);
    settings.sessionFilePath_ = sessionFilePathForProfile(settings.profile_);
    settings.secretsFilePath_ = secretsFilePathForProfile(settings.profile_);
    createDir(settings.profileDirPath_);

    settings.setPersistenceSuspended(true);

    const auto effectiveConfig =
      configRoot.IsDefined() ? configRoot : loadYamlFile(settings.configFilePath_, "config");
    settings.loadConfigYaml(effectiveConfig);

    const auto provider =
      providerFromConfig(effectiveConfig, settings.runWithoutSecureSecretsService_);
    settings.runWithoutSecureSecretsService_ = provider == staged_load_plan::SecretsProvider::File;

    for (const auto stage : staged_load_plan::stagesForProvider(provider)) {
        switch (stage) {
        case staged_load_plan::Stage::Config:
            break;
        case staged_load_plan::Stage::Session: {
            const auto sessionRoot = loadYamlFile(settings.sessionFilePath_, "session");
            settings.loadSessionYaml(sessionRoot);
            break;
        }
        case staged_load_plan::Stage::SecretsSecureBackend:
        case staged_load_plan::Stage::SecretsFile: {
            const auto payload =
              settings::persistence::loadProfileSecrets(settings.profile_,
                                                        settings.runWithoutSecureSecretsService_,
                                                        settings.secretsFilePath_);
            settings.accessToken_ = payload.accessToken;
            settings.secrets_     = payload.secrets;

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
            const auto stateRoot = loadYamlFile(settings.stateFilePath_, "state");
            settings.loadStateYaml(stateRoot);
            break;
        }
        }
    }

    settings.applyTheme();
    settings.setPersistenceScopeReadyForAuth(settings.hasActiveSession());
    // Keep persistence intentionally paused until the UI startup sequence completes.
    // This avoids incidental `save()` calls from initialization code paths.
    settings.setPersistenceSuspended(true);

    if (profile)
        emit settings.profileChanged(settings.profile_);
}

void
settings::SettingsController::save(UserSettings &settings, SavePolicy policy)
{
    if (settings.profileDirPath_.isEmpty()) {
        settings.profileDirPath_  = profileDirPath(settings.profile_);
        settings.configFilePath_  = configFilePathForProfile(settings.profile_);
        settings.stateFilePath_   = stateFilePathForProfile(settings.profile_);
        settings.sessionFilePath_ = sessionFilePathForProfile(settings.profile_);
        settings.secretsFilePath_ = secretsFilePathForProfile(settings.profile_);
        createDir(settings.profileDirPath_);
    }

    settings.saveConfigYaml();
    if (policy == SavePolicy::Full) {
        settings.saveSessionYaml();
        settings.saveSecretsYaml();
        settings.saveStateYaml();
    }
}

void
settings::SettingsController::clearAuth(UserSettings &settings)
{
    activeLoggers().ui->info("Clearing persisted session auth/identity for profile '{}'",
                             app_paths::normalizedProfileId(settings.profile_).toStdString());

    settings.accessToken_ = QString();
    settings.homeserver_  = QString();
    settings.userId_      = QString();
    settings.deviceId_    = QString();
    settings.secrets_.clear();

    if (pathExists(settings.sessionFilePath_) && !removePath(settings.sessionFilePath_)) {
        activeLoggers().ui->warn("Failed to remove session file '{}', keeping file to avoid "
                                 "accidental data loss",
                                 settings.sessionFilePath_.toStdString());
    }

    settings::persistence::clearProfileSecrets(
      settings.profile_, settings.runWithoutSecureSecretsService_, settings.secretsFilePath_);
    settings.saveStateYaml();
}
