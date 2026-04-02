// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsController.h"
#include "SettingsControllerInternal.h"
#include "komai-rust-cxxbridge/ffi.h"

#include <utility>

#include "logging/Logging.h"

#include "profile/Paths.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsSerializer.h"
#include "settings/SettingsStorage.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

using settings::storage::createDir;
using settings::storage::pathExists;
using settings::storage::removePath;

::komai::rust::SettingsProfileHandle *
ensureRustSettingsProfileHandle(UserSettings &settings, bool includeSession)
{
    if (auto *handle = settings.rustSettingsProfileHandle(); handle != nullptr)
        return handle;

    auto handle = ::komai::rust::settings_open_profile_handle_for_profile(
      settings.profileId().toStdString(), includeSession);
    settings.setRustSettingsProfileHandle(std::move(handle));
    return settings.rustSettingsProfileHandle();
}

} // namespace

void
settings::SettingsController::save(UserSettings &settings, SavePolicy policy)
{
    if (!settings.hasResolvedProfilePaths()) {
        settings.applyProfilePathState(settings.profileId());
        createDir(settings.profileDirPath());
    }

    syncCoreStoreFromSettings(settings);

    if (policy == SavePolicy::ConfigOnly) {
        auto *profileHandle = ensureRustSettingsProfileHandle(settings, false);
        settings::serializer::saveConfig(
          settings, settings.configFilePath(), settings.usesFileSecretsProvider(), *profileHandle);
    } else if (policy == SavePolicy::StateOnly) {
        auto *profileHandle = ensureRustSettingsProfileHandle(settings, false);
        settings::serializer::saveState(settings, settings.stateFilePath(), *profileHandle);
    } else if (policy == SavePolicy::Full) {
        auto *profileHandle = ensureRustSettingsProfileHandle(settings, true);
        settings::serializer::saveConfig(
          settings, settings.configFilePath(), settings.usesFileSecretsProvider(), *profileHandle);
        settings::serializer::saveSession(settings, settings.sessionFilePath(), *profileHandle);

        settings::persistence::saveProfileSecrets(settings.profileId(),
                                                  settings.usesFileSecretsProvider(),
                                                  settings.secretsFilePath(),
                                                  settings.accessToken(),
                                                  settings.secretsMap());

        settings::serializer::saveState(settings, settings.stateFilePath(), *profileHandle);
    }
    syncCoreStoreFromSettings(settings);
}

void
settings::SettingsController::clearAuth(UserSettings &settings)
{
    activeLoggers().ui->info("Clearing persisted session auth/identity for profile '{}'",
                             app_paths::normalizedProfileId(settings.profileId()).toStdString());

    settings.clearAuthInMemory();
    settings.clearRustSettingsProfileHandle();

    if (pathExists(settings.sessionFilePath()) && !removePath(settings.sessionFilePath())) {
        activeLoggers().ui->warn("Failed to remove session file '{}', keeping file to avoid "
                                 "accidental data loss",
                                 settings.sessionFilePath().toStdString());
    }

    settings::persistence::clearProfileSecrets(
      settings.profileId(), settings.usesFileSecretsProvider(), settings.secretsFilePath());
    auto *profileHandle = ensureRustSettingsProfileHandle(settings, false);
    settings::serializer::saveState(settings, settings.stateFilePath(), *profileHandle);
    syncCoreStoreFromSettings(settings);
}
