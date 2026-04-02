// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"

#include "profile/Paths.h"

namespace settings::persistence {

void
saveProfileSecrets(const QString &profile,
                   bool usesFileSecretsProvider,
                   const QString &accessToken,
                   const QMap<QString, QString> &secrets)
{
    const bool saved =
      detail::saveProfileSecretsPayload(profile, usesFileSecretsProvider, accessToken, secrets);
    if (usesFileSecretsProvider && saved) {
        activeLoggers().ui->debug("Saved secrets for profile '{}'",
                                  app_paths::normalizedProfileId(profile).toStdString());
    }
}

bool
clearProfileSecrets(const QString &profile, bool usesFileSecretsProvider)
{
    if (usesFileSecretsProvider) {
        const auto normalizedProfile = app_paths::normalizedProfileId(profile);
        if (!::komai::rust::settings_clear_profile_secrets(profile.toStdString(), true)) {
            activeLoggers().ui->warn("Failed to remove file-backed secrets for profile '{}'",
                                     normalizedProfile.toStdString());
            return false;
        }
        activeLoggers().ui->info("Cleared file-backed secrets for profile '{}'",
                                 normalizedProfile.toStdString());
        return true;
    }

    const auto normalizedProfile = app_paths::normalizedProfileId(profile);
    const auto allSecretsDeleted =
      ::komai::rust::settings_clear_profile_secrets(profile.toStdString(), false);
    if (!allSecretsDeleted) {
        activeLoggers().ui->warn(
          "Failed to delete all profile secrets during logout for profile '{}'",
          normalizedProfile.toStdString());
    }

    return allSecretsDeleted;
}

} // namespace settings::persistence
