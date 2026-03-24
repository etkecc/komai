// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixLegacySession.h"

#include "settings/SettingKeys.h"
#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"
#include "settings/YamlSettings.h"

namespace {

bool
usesFileSecretsProvider(const QString &profileId)
{
    const auto configFilePath = settings::storage::configFilePathForProfile(profileId);
    const auto configRoot     = settings::storage::loadYamlFile(configFilePath, "config");
    const auto provider       = settings::persistence::providerFromConfig(configRoot);

    return provider == staged_load_plan::SecretsProvider::File;
}

QString
readNormalizedSessionValue(const YAML::Node &root, const char *key)
{
    return yaml_settings::readString(root, key, QString{}).trimmed();
}

} // namespace

namespace komai::matrix_backend {

bool
PersistedLegacyMatrixSession::hasCompleteSession() const
{
    return !userId.trimmed().isEmpty() && !deviceId.trimmed().isEmpty() &&
           !homeserverUrl.trimmed().isEmpty() && !accessToken.trimmed().isEmpty();
}

PersistedLegacyMatrixSession
loadPersistedLegacyMatrixSession(const QString &profileId)
{
    const auto sessionFilePath = settings::storage::sessionFilePathForProfile(profileId);
    const auto sessionRoot     = settings::storage::loadYamlFile(sessionFilePath, "session");
    const auto secretsFilePath = settings::storage::secretsFilePathForProfile(profileId);
    const auto secretsPayload  = settings::persistence::loadProfileSecrets(
      profileId, usesFileSecretsProvider(profileId), secretsFilePath);

    return {
      .userId   = readNormalizedSessionValue(sessionRoot, SettingKey::SessionAccountUserId),
      .deviceId = readNormalizedSessionValue(sessionRoot, SettingKey::SessionDeviceId),
      .homeserverUrl =
        readNormalizedSessionValue(sessionRoot, SettingKey::SessionAccountHomeserver),
      .accessToken = secretsPayload.accessToken.trimmed(),
    };
}

} // namespace komai::matrix_backend
