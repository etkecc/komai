// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include <spdlog/logger.h>

#include "Paths.h"
#include "ProfileSecrets.h"
#include "settings/SettingsStorage.h"
#include "settings/YamlSettings.h"

namespace settings::persistence {

void
saveProfileSecrets(const QString &profile,
                   bool usesFileSecretsProvider,
                   const QString &secretsFilePath,
                   const QString &accessToken,
                   const QMap<QString, QString> &secrets,
                   const QString &sessionUserId,
                   const QString &sessionDeviceId,
                   const QString &sessionHomeserver)
{
    auto secretsWithSessionMetadata = secrets;
    detail::storeInternalSessionMetadata(
      secretsWithSessionMetadata, sessionUserId, sessionDeviceId, sessionHomeserver);

    if (usesFileSecretsProvider) {
        YAML::Node root(YAML::NodeType::Map);
        yaml_settings::setNode(
          root, SettingKey::SecretsFileAuthAccessToken, accessToken.toStdString());
        yaml_settings::writeStringMap(root, SettingKey::SecretsFileMap, secretsWithSessionMetadata);

        if (settings::storage::writeYamlFile(secretsFilePath, root, true)) {
            activeLoggers().ui->debug("Saved secrets to: {}", secretsFilePath.toStdString());
        }
        return;
    }

    const auto accessTokenKey =
      settings::storage::secureStoreKey(profile, SecureStoreAccessTokenKey);
    const auto secretsKey = settings::storage::secureStoreKey(profile, SecureStoreSecretsKey);
    QMap<QString, QString> nonEmptySecrets = secretsWithSessionMetadata;

    for (auto it = nonEmptySecrets.begin(); it != nonEmptySecrets.end();) {
        if (it.value().isEmpty())
            it = nonEmptySecrets.erase(it);
        else
            ++it;
    }

    if (accessToken.isEmpty())
        settings::storage::deleteSecureValue(accessTokenKey);
    else
        settings::storage::writeSecureValue(accessTokenKey, accessToken);

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
