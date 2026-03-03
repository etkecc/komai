// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include <spdlog/logger.h>

#include "profile/Paths.h"
#include "profile/ProfileSecrets.h"
#include "settings/SettingsStorage.h"
#include "settings/YamlSettings.h"

namespace settings::persistence {

void
saveProfileSecrets(const QString &profile,
                   bool usesFileSecretsProvider,
                   const QString &secretsFilePath,
                   const QString &accessToken,
                   const QMap<QString, QString> &secrets)
{
    auto secretsWithSessionMetadata = secrets;
    detail::storeInternalSessionMetadata(secretsWithSessionMetadata, accessToken);
    QMap<QString, QString> nonEmptySecrets = secretsWithSessionMetadata;

    for (auto it = nonEmptySecrets.begin(); it != nonEmptySecrets.end();) {
        if (it.value().isEmpty())
            it = nonEmptySecrets.erase(it);
        else
            ++it;
    }

    if (usesFileSecretsProvider) {
        YAML::Node root(YAML::NodeType::Map);
        yaml_settings::writeStringMap(root, SettingKey::SecretsFileMap, nonEmptySecrets);

        if (settings::storage::writeYamlFile(secretsFilePath, root, true)) {
            activeLoggers().ui->debug("Saved secrets to: {}", secretsFilePath.toStdString());
        }
        return;
    }

    const auto secretsKey = settings::storage::secureStoreKey(profile, SecureStoreSecretsKey);

    if (nonEmptySecrets.isEmpty())
        settings::storage::deleteSecureValue(secretsKey);
    else
        settings::storage::writeSecureValue(secretsKey,
                                            settings::storage::encodeSecretsMap(nonEmptySecrets));

    if (settings::storage::pathExists(secretsFilePath) &&
        !settings::storage::removePath(secretsFilePath))
        activeLoggers().ui->warn("Failed to remove stale secrets file: {}",
                                 secretsFilePath.toStdString());
}

bool
clearProfileSecrets(const QString &profile,
                    bool usesFileSecretsProvider,
                    const QString &secretsFilePath)
{
    if (usesFileSecretsProvider) {
        const auto normalizedProfile = app_paths::normalizedProfileId(profile);
        if (settings::storage::pathExists(secretsFilePath) &&
            !settings::storage::removePath(secretsFilePath)) {
            activeLoggers().ui->warn("Failed to remove stale secrets file: {}",
                                     secretsFilePath.toStdString());
            return false;
        }
        activeLoggers().ui->info("Cleared file-backed secrets for profile '{}'",
                                 normalizedProfile.toStdString());
        return true;
    }

    const auto normalizedProfile = app_paths::normalizedProfileId(profile);
    const auto allSecretsDeleted =
      profile_secrets::deleteAllProfileSecretsFromStoreBlocking(profile);
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
