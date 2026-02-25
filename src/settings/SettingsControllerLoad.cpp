// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsController.h"
#include "SettingsControllerInternal.h"

#include <yaml-cpp/yaml.h>

#include "settings/SettingsPersistence.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsStorage.h"
#include "settings/StagedLoadPlan.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

using settings::storage::createDir;
using settings::storage::loadYamlFile;

} // namespace

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

    const bool hasInjectedConfig = configRoot.IsDefined() && !configRoot.IsNull();
    const auto effectiveConfig =
      hasInjectedConfig ? configRoot : loadYamlFile(settings.configFilePath(), "config");
    settings::serializer::loadConfig(settings, effectiveConfig);

    const auto provider = settings::persistence::providerFromConfig(effectiveConfig);
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
    // Keep the core store synchronized from validated runtime settings only.
    // Avoid re-importing raw YAML scalars here, because that can bypass token
    // conversion/validation paths and reintroduce invalid persisted values.
    syncCoreStoreFromSettings(settings);
    settings.setPersistenceScopeReadyForAuth(settings.hasActiveSession());
    // Keep persistence intentionally paused until the UI startup sequence completes.
    // This avoids incidental `save()` calls from initialization code paths.
    settings.setPersistenceSuspended(true);

    if (profile)
        settings.notifyProfileChanged();
}
