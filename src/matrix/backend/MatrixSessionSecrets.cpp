// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixSessionSecrets.h"
#include "komai-rust-cxxbridge/lib.h"

#include <QDir>
#include <QHash>
#include <QMap>

#include "settings/SettingsPersistence.h"
#include "settings/SettingsStorage.h"

namespace {

constexpr auto MatrixSdkStorePassphraseKey   = "matrix_sdk.store_passphrase";
constexpr auto MatrixSdkHomeserverUrlKey     = "matrix_sdk.homeserver_url";
constexpr auto MatrixSdkSerializedSessionKey = "matrix_sdk.serialized_session";
constexpr auto MatrixSdkSecureStoreKey       = "matrix_sdk.session";
constexpr auto MatrixSdkSecretsFileName      = "matrix-sdk-secrets.yml";

struct SecretsPersistenceContext
{
    bool usesFileSecretsProvider = false;
    QString secretsFilePath;
    QString secureStoreKey;
};

SecretsPersistenceContext
loadSecretsPersistenceContext(const QString &profileId)
{
    static QHash<QString, SecretsPersistenceContext> cache;

    auto it = cache.constFind(profileId);
    if (it != cache.constEnd())
        return *it;

    const auto configFilePath = settings::storage::configFilePathForProfile(profileId);
    const auto config         = ::komai::rust::settings_load_config_overview(
      settings::storage::readTextFile(configFilePath, "config").toStdString());
    const auto provider = settings::persistence::providerFromConfigValue(
      QString::fromStdString(static_cast<std::string>(config.secrets_provider)));

    SecretsPersistenceContext context{
      .usesFileSecretsProvider = provider == staged_load_plan::SecretsProvider::File,
      .secretsFilePath         = QDir(settings::storage::profileDirPath(profileId))
                           .filePath(QString::fromLatin1(MatrixSdkSecretsFileName)),
      .secureStoreKey = settings::storage::secureStoreKey(profileId, MatrixSdkSecureStoreKey),
    };

    cache.insert(profileId, context);
    return context;
}

QMap<QString, QString>
loadStoredMatrixSdkSecrets(const SecretsPersistenceContext &context)
{
    if (context.usesFileSecretsProvider) {
        const auto serialized =
          settings::storage::readTextFile(context.secretsFilePath, "matrix-sdk secrets");
        return settings::storage::decodeSecretsFilePayload(serialized);
    }

    const auto serializedSecrets = settings::storage::readSecureValue(context.secureStoreKey);
    if (!serializedSecrets.has_value() || serializedSecrets->isEmpty())
        return {};

    return settings::storage::decodeSecretsMap(*serializedSecrets);
}

void
saveStoredMatrixSdkSecrets(const SecretsPersistenceContext &context,
                           const QMap<QString, QString> &secrets)
{
    if (context.usesFileSecretsProvider) {
        if (secrets.isEmpty()) {
            settings::storage::removePath(context.secretsFilePath);
            return;
        }

        settings::storage::writeTextFile(
          context.secretsFilePath, settings::storage::encodeSecretsFilePayload(secrets), true);
        return;
    }

    if (secrets.isEmpty()) {
        settings::storage::deleteSecureValueBlocking(context.secureStoreKey);
        return;
    }

    settings::storage::writeSecureValueBlocking(context.secureStoreKey,
                                                settings::storage::encodeSecretsMap(secrets));
}

} // namespace

namespace komai::matrix_backend {

PersistedMatrixSessionSecrets
loadPersistedMatrixSessionSecrets(const QString &profileId)
{
    const auto context = loadSecretsPersistenceContext(profileId);
    const auto secrets = loadStoredMatrixSdkSecrets(context);

    return {
      .storePassphrase   = secrets.value(MatrixSdkStorePassphraseKey),
      .homeserverUrl     = secrets.value(MatrixSdkHomeserverUrlKey),
      .serializedSession = secrets.value(MatrixSdkSerializedSessionKey),
    };
}

void
savePersistedMatrixSessionSecrets(const QString &profileId,
                                  const PersistedMatrixSessionSecrets &secrets)
{
    const auto context = loadSecretsPersistenceContext(profileId);
    QMap<QString, QString> storedSecrets;

    if (secrets.storePassphrase.isEmpty())
        storedSecrets.remove(MatrixSdkStorePassphraseKey);
    else
        storedSecrets[MatrixSdkStorePassphraseKey] = secrets.storePassphrase;

    if (secrets.homeserverUrl.isEmpty())
        storedSecrets.remove(MatrixSdkHomeserverUrlKey);
    else
        storedSecrets[MatrixSdkHomeserverUrlKey] = secrets.homeserverUrl;

    if (secrets.serializedSession.isEmpty())
        storedSecrets.remove(MatrixSdkSerializedSessionKey);
    else
        storedSecrets[MatrixSdkSerializedSessionKey] = secrets.serializedSession;

    saveStoredMatrixSdkSecrets(context, storedSecrets);
}

void
clearPersistedMatrixSessionSecrets(const QString &profileId)
{
    savePersistedMatrixSessionSecrets(profileId, {});
}

} // namespace komai::matrix_backend
