// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include <QString>

#include <spdlog/logger.h>

#include "Paths.h"
#include "ProfileSecrets.h"
#include "settings/SettingsStorage.h"
#include "settings/YamlSettings.h"

namespace settings::persistence {

SecretsPayload
loadProfileSecrets(const QString &profile,
                   bool usesFileSecretsProvider,
                   const QString &secretsFilePath)
{
    SecretsPayload payload;
    bool hasEmptySecureSecrets   = false;
    const auto normalizedProfile = app_paths::normalizedProfileId(profile);

    if (usesFileSecretsProvider) {
        const auto secretsRoot = settings::storage::loadYamlFile(secretsFilePath, "secrets");
        payload.accessToken =
          yaml_settings::readString(secretsRoot, SettingKey::SecretsFileAuthAccessToken, QString());
        payload.secrets = yaml_settings::readStringMap(secretsRoot, SettingKey::SecretsFileMap);
        detail::extractInternalSessionMetadata(payload);

        activeLoggers().ui->info(
          "Loaded file-backed secrets (has_access_token={}, secrets_count={})",
          !payload.accessToken.trimmed().isEmpty(),
          payload.secrets.size());
        return payload;
    }

    const auto accessTokenStoreKey =
      settings::storage::secureStoreKey(profile, SecureStoreAccessTokenKey);
    const auto secureAccessToken = settings::storage::readSecureValue(accessTokenStoreKey);
    if (secureAccessToken && secureAccessToken->isEmpty()) {
        activeLoggers().ui->warn("Secure backend access token was empty; "
                                 "removing stale session auth secret for profile '{}'",
                                 normalizedProfile.toStdString());
        const auto staleAccessTokenDeleted =
          profile_secrets::deleteProfileSecretValueBlocking(accessTokenStoreKey);
        if (!staleAccessTokenDeleted) {
            activeLoggers().ui->warn(
              "Failed to remove stale secure backend session auth secret for profile '{}'",
              normalizedProfile.toStdString());
        }
        hasEmptySecureSecrets = true;
    } else {
        payload.accessToken = secureAccessToken.value_or(QString());
    }

    const auto secretsStoreKey = settings::storage::secureStoreKey(profile, SecureStoreSecretsKey);
    const auto serializedSecrets = settings::storage::readSecureValue(secretsStoreKey);
    if (serializedSecrets && serializedSecrets->isEmpty()) {
        activeLoggers().ui->warn("Secure backend secrets payload was empty; "
                                 "removing stale secret storage for profile '{}'",
                                 normalizedProfile.toStdString());
        const auto staleSecretsDeleted =
          profile_secrets::deleteProfileSecretValueBlocking(secretsStoreKey);
        if (!staleSecretsDeleted) {
            activeLoggers().ui->warn(
              "Failed to remove stale secure backend session secrets for profile '{}'",
              normalizedProfile.toStdString());
        }
        hasEmptySecureSecrets = true;
    } else {
        payload.secrets = serializedSecrets
                            ? settings::storage::decodeSecretsMap(*serializedSecrets)
                            : QMap<QString, QString>{};
        detail::extractInternalSessionMetadata(payload);

        bool sessionSecretsPruned = false;
        for (auto it = payload.secrets.begin(); it != payload.secrets.end();) {
            if (it.value().isEmpty()) {
                activeLoggers().ui->warn("Pruning empty secure secret entry '{}' for profile '{}'",
                                         it.key().toStdString(),
                                         normalizedProfile.toStdString());
                it                   = payload.secrets.erase(it);
                sessionSecretsPruned = true;
            } else {
                ++it;
            }
        }

        if (sessionSecretsPruned) {
            if (payload.secrets.isEmpty()) {
                const auto staleSecretsDeleted =
                  profile_secrets::deleteProfileSecretValueBlocking(secretsStoreKey);
                if (!staleSecretsDeleted) {
                    activeLoggers().ui->warn(
                      "Failed to remove stale secure backend session secrets for profile '{}'",
                      normalizedProfile.toStdString());
                }
            } else {
                settings::storage::writeSecureValue(
                  secretsStoreKey, settings::storage::encodeSecretsMap(payload.secrets));
            }
            hasEmptySecureSecrets = true;
        }
    }

    if (hasEmptySecureSecrets) {
        activeLoggers().ui->warn("Found stale/empty secure backend values for profile '{}'",
                                 normalizedProfile.toStdString());
    }

    activeLoggers().ui->info(
      "Loaded secure-backend secrets (has_access_token={}, secrets_count={})",
      !payload.accessToken.trimmed().isEmpty(),
      payload.secrets.size());

    payload.hadStaleValues = hasEmptySecureSecrets;
    return payload;
}

} // namespace settings::persistence
