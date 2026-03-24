// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixSessionSecrets.h"

#include <QMap>

#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"

namespace {

constexpr auto MatrixSdkStorePassphraseKey   = "matrix_sdk.store_passphrase";
constexpr auto MatrixSdkHomeserverUrlKey     = "matrix_sdk.homeserver_url";
constexpr auto MatrixSdkSerializedSessionKey = "matrix_sdk.serialized_session";

struct SecretsPersistenceContext
{
    bool usesFileSecretsProvider = false;
    QString secretsFilePath;
};

SecretsPersistenceContext
loadSecretsPersistenceContext(const QString &profileId)
{
    const auto configFilePath = settings::storage::configFilePathForProfile(profileId);
    const auto configRoot     = settings::storage::loadYamlFile(configFilePath, "config");
    const auto provider       = settings::persistence::providerFromConfig(configRoot);

    return {
      .usesFileSecretsProvider = provider == staged_load_plan::SecretsProvider::File,
      .secretsFilePath         = settings::storage::secretsFilePathForProfile(profileId),
    };
}

} // namespace

namespace komai::matrix_backend {

PersistedMatrixSessionSecrets
loadPersistedMatrixSessionSecrets(const QString &profileId)
{
    const auto context = loadSecretsPersistenceContext(profileId);
    const auto payload = settings::persistence::loadProfileSecrets(
      profileId, context.usesFileSecretsProvider, context.secretsFilePath);

    return {
      .storePassphrase   = payload.secrets.value(MatrixSdkStorePassphraseKey),
      .homeserverUrl     = payload.secrets.value(MatrixSdkHomeserverUrlKey),
      .serializedSession = payload.secrets.value(MatrixSdkSerializedSessionKey),
    };
}

void
savePersistedMatrixSessionSecrets(const QString &profileId,
                                  const PersistedMatrixSessionSecrets &secrets)
{
    const auto context = loadSecretsPersistenceContext(profileId);
    auto payload       = settings::persistence::loadProfileSecrets(
      profileId, context.usesFileSecretsProvider, context.secretsFilePath);

    if (secrets.storePassphrase.isEmpty())
        payload.secrets.remove(MatrixSdkStorePassphraseKey);
    else
        payload.secrets[MatrixSdkStorePassphraseKey] = secrets.storePassphrase;

    if (secrets.homeserverUrl.isEmpty())
        payload.secrets.remove(MatrixSdkHomeserverUrlKey);
    else
        payload.secrets[MatrixSdkHomeserverUrlKey] = secrets.homeserverUrl;

    if (secrets.serializedSession.isEmpty())
        payload.secrets.remove(MatrixSdkSerializedSessionKey);
    else
        payload.secrets[MatrixSdkSerializedSessionKey] = secrets.serializedSession;

    settings::persistence::saveProfileSecrets(profileId,
                                              context.usesFileSecretsProvider,
                                              context.secretsFilePath,
                                              payload.accessToken,
                                              payload.secrets);
}

void
clearPersistedMatrixSessionSecrets(const QString &profileId)
{
    savePersistedMatrixSessionSecrets(profileId, {});
}

} // namespace komai::matrix_backend
