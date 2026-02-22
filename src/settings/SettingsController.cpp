// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QDir>

#include <yaml-cpp/yaml.h>

#include "SettingsController.h"

#include "Logging.h"
#include "Paths.h"
#include "UserSettingsPage.h"
#include "settings/SettingKeys.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"

namespace {

using settings::storage::configFilePathForProfile;
using settings::storage::loadYamlFile;
using settings::storage::profileDirPath;
using settings::storage::secretsFilePathForProfile;
using settings::storage::sessionFilePathForProfile;
using settings::storage::stateFilePathForProfile;

using settings::persistence::providerFromConfig;

} // namespace

void
settings::SettingsController::load(UserSettings &settings, std::optional<QString> profile)
{
    if (profile)
        settings.profile_ = (*profile == QLatin1String("default")) ? QLatin1String("") : *profile;
    else
        settings.profile_ = QLatin1String("");

    settings.profileDirPath_ = profileDirPath(settings.profile_);
    settings.configFilePath_ = configFilePathForProfile(settings.profile_);
    settings.stateFilePath_  = stateFilePathForProfile(settings.profile_);
    settings.sessionFilePath_ = sessionFilePathForProfile(settings.profile_);
    settings.secretsFilePath_ = secretsFilePathForProfile(settings.profile_);
    QDir().mkpath(settings.profileDirPath_);

    const auto configRoot = loadYamlFile(settings.configFilePath_, "config");
    settings.loadConfigYaml(configRoot);

    const auto provider = providerFromConfig(configRoot, settings.runWithoutSecureSecretsService_);
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

    if (profile)
        emit settings.profileChanged(settings.profile_);
}

void
settings::SettingsController::save(UserSettings &settings)
{
    if (settings.profileDirPath_.isEmpty()) {
        settings.profileDirPath_ = profileDirPath(settings.profile_);
        settings.configFilePath_ = configFilePathForProfile(settings.profile_);
        settings.stateFilePath_ = stateFilePathForProfile(settings.profile_);
        settings.sessionFilePath_ = sessionFilePathForProfile(settings.profile_);
        settings.secretsFilePath_ = secretsFilePathForProfile(settings.profile_);
        QDir().mkpath(settings.profileDirPath_);
    }

    settings.saveConfigYaml();
    settings.saveSessionYaml();
    settings.saveSecretsYaml();
    settings.saveStateYaml();
}

void
settings::SettingsController::clearAuth(UserSettings &settings)
{
    nhlog::ui()->info("Clearing persisted session auth/identity for profile '{}'",
                      app_paths::normalizedProfileId(settings.profile_).toStdString());

    settings.accessToken_ = QString();
    settings.homeserver_  = QString();
    settings.userId_      = QString();
    settings.deviceId_    = QString();
    settings.secrets_.clear();

    settings.saveSessionYaml();
    settings::persistence::clearProfileSecrets(settings.profile_,
                                              settings.runWithoutSecureSecretsService_,
                                              settings.secretsFilePath_);
    settings.saveStateYaml();
}
