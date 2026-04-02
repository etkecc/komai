// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include "logging/Logging.h"

#include "profile/Paths.h"
#include "profile/ProfileSecrets.h"
#include "settings/SettingsStorage.h"

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
        if (!detail::removePersistedSecretsFileForProfile(profile)) {
            activeLoggers().ui->warn("Failed to remove file-backed secrets for profile '{}'",
                                     normalizedProfile.toStdString());
            return false;
        }
        activeLoggers().ui->info("Cleared file-backed secrets for profile '{}'",
                                 normalizedProfile.toStdString());
        return true;
    }

    const auto secretsFilePath   = settings::storage::secretsFilePathForProfile(profile);
    const auto normalizedProfile = app_paths::normalizedProfileId(profile);
    const auto provider          = usesFileSecretsProvider
                                     ? staged_load_plan::SecretsProvider::File
                                     : staged_load_plan::SecretsProvider::SecretService;
    const auto allSecretsDeleted =
      profile_secrets::deleteAllProfileSecretsFromStoreBlocking(profile, provider);
    if (!allSecretsDeleted) {
        activeLoggers().ui->warn(
          "Failed to delete all profile secrets during logout for profile '{}'",
          normalizedProfile.toStdString());
    }

    if (settings::storage::pathExists(secretsFilePath) &&
        !settings::storage::removePath(secretsFilePath)) {
        activeLoggers().ui->warn("Failed to remove stale secrets file: {}",
                                 secretsFilePath.toStdString());
        return false;
    }

    return allSecretsDeleted;
}

} // namespace settings::persistence
