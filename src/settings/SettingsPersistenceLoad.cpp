// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include <QString>

#include <spdlog/logger.h>

#include "Paths.h"
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
        payload.secrets = yaml_settings::readStringMap(secretsRoot, SettingKey::SecretsFileMap);
        detail::extractInternalSessionMetadata(payload);

        activeLoggers().ui->info(
          "Loaded file-backed secrets (has_access_token={}, secrets_count={})",
          !payload.accessToken.trimmed().isEmpty(),
          payload.secrets.size());
        return payload;
    }

    const auto secretsStoreKey = settings::storage::secureStoreKey(profile, SecureStoreSecretsKey);
    const auto serializedSecrets = settings::storage::readSecureValue(secretsStoreKey);
    if (serializedSecrets && serializedSecrets->isEmpty()) {
        activeLoggers().ui->warn("Secure backend secrets payload was empty; "
                                 "removing stale secret storage for profile '{}'",
                                 normalizedProfile.toStdString());
        settings::storage::deleteSecureValue(secretsStoreKey);
        hasEmptySecureSecrets = true;
    } else {
        QMap<QString, QString> decodedSecrets =
          serializedSecrets ? settings::storage::decodeSecretsMap(*serializedSecrets)
                            : QMap<QString, QString>{};
        bool sessionSecretsPruned = false;
        for (auto it = decodedSecrets.begin(); it != decodedSecrets.end();) {
            if (it.value().isEmpty()) {
                activeLoggers().ui->warn("Pruning empty secure secret entry '{}' for profile '{}'",
                                         it.key().toStdString(),
                                         normalizedProfile.toStdString());
                it                   = decodedSecrets.erase(it);
                sessionSecretsPruned = true;
            } else {
                ++it;
            }
        }

        payload.secrets = decodedSecrets;
        detail::extractInternalSessionMetadata(payload);

        if (sessionSecretsPruned) {
            if (decodedSecrets.isEmpty())
                settings::storage::deleteSecureValue(secretsStoreKey);
            else
                settings::storage::writeSecureValue(
                  secretsStoreKey, settings::storage::encodeSecretsMap(decodedSecrets));
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
