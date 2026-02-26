// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsController.h"
#include "SettingsControllerInternal.h"

#include <spdlog/logger.h>

#include "Paths.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsStorage.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

using settings::storage::createDir;
using settings::storage::pathExists;
using settings::storage::removePath;

} // namespace

void
settings::SettingsController::save(UserSettings &settings, SavePolicy policy)
{
    if (!settings.hasResolvedProfilePaths()) {
        settings.applyProfilePathState(settings.profileId());
        createDir(settings.profileDirPath());
    }

    syncCoreStoreFromSettings(settings);

    settings::serializer::saveConfig(
      settings, settings.configFilePath(), settings.usesFileSecretsProvider());

    if (policy == SavePolicy::Full) {
        settings::serializer::saveSession(settings, settings.sessionFilePath());

        settings::persistence::saveProfileSecrets(settings.profileId(),
                                                  settings.usesFileSecretsProvider(),
                                                  settings.secretsFilePath(),
                                                  settings.accessToken(),
                                                  settings.secretsMap());

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

    settings::persistence::clearProfileSecrets(
      settings.profileId(), settings.usesFileSecretsProvider(), settings.secretsFilePath());
    settings::serializer::saveState(settings, settings.stateFilePath());
    syncCoreStoreFromSettings(settings);
}
