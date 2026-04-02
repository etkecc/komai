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
    }

    syncCoreStoreFromSettings(settings);

    if (policy == SavePolicy::ConfigOnly) {
        auto *profileHandle = ensureRustSettingsProfileHandle(settings, false);
        settings::serializer::saveConfig(
          settings, settings.usesFileSecretsProvider(), *profileHandle);
    } else if (policy == SavePolicy::StateOnly) {
        auto *profileHandle = ensureRustSettingsProfileHandle(settings, false);
        settings::serializer::saveState(settings, *profileHandle);
    } else if (policy == SavePolicy::Full) {
        auto *profileHandle = ensureRustSettingsProfileHandle(settings, true);
        settings::serializer::saveConfig(
          settings, settings.usesFileSecretsProvider(), *profileHandle);
        settings::serializer::saveSession(settings, *profileHandle);

        settings::persistence::saveProfileSecrets(settings.profileId(),
                                                  settings.usesFileSecretsProvider(),
                                                  settings.accessToken(),
                                                  settings.secretsMap());

        settings::serializer::saveState(settings, *profileHandle);
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

    if (!::komai::rust::settings_remove_session_file_for_profile(
          settings.profileId().toStdString())) {
        activeLoggers().ui->warn(
          "Failed to remove persisted session file for profile '{}', keeping file to avoid "
          "accidental data loss",
          app_paths::normalizedProfileId(settings.profileId()).toStdString());
    }

    settings::persistence::clearProfileSecrets(settings.profileId(),
                                               settings.usesFileSecretsProvider());
    auto *profileHandle = ensureRustSettingsProfileHandle(settings, false);
    settings::serializer::saveState(settings, *profileHandle);
    syncCoreStoreFromSettings(settings);
}
