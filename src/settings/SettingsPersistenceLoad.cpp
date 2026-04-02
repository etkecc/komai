// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/SettingsPersistence.h"
#include "settings/SettingsPersistenceInternal.h"

#include <QString>

#include "logging/Logging.h"

#include "profile/Paths.h"
#include "settings/SettingsStorage.h"

namespace settings::persistence {

SecretsPayload
loadProfileSecrets(const QString &profile, bool usesFileSecretsProvider)
{
    SecretsPayload payload;
    bool hasEmptySecureSecrets   = false;
    const auto normalizedProfile = app_paths::normalizedProfileId(profile);

    if (usesFileSecretsProvider) {
        payload = detail::loadPersistedSecretsFilePayloadForProfile(profile);
        if (payload.hadStaleValues) {
            saveProfileSecrets(profile, true, payload.accessToken, payload.secrets);
        }

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
        payload = serializedSecrets ? detail::decodePersistedSecretsMap(*serializedSecrets)
                                    : SecretsPayload{};

        if (payload.hadStaleValues) {
            saveProfileSecrets(profile, false, payload.accessToken, payload.secrets);
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
