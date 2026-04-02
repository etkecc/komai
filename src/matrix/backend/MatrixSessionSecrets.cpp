// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixSessionSecrets.h"
#include "komai-rust-cxxbridge/ffi.h"

#include <QHash>
#include <QMap>

#include "settings/SettingsStorage.h"

namespace {

constexpr auto MatrixSdkStorePassphraseKey   = "matrix_sdk.store_passphrase";
constexpr auto MatrixSdkHomeserverUrlKey     = "matrix_sdk.homeserver_url";
constexpr auto MatrixSdkSerializedSessionKey = "matrix_sdk.serialized_session";
constexpr auto MatrixSdkSecureStoreKey       = "matrix_sdk.session";

struct SecretsPersistenceContext
{
    bool usesFileSecretsProvider = false;
    QString secureStoreKey;
};

::rust::Vec<::komai::rust::SettingsStringMapEntry>
toRustStringMapEntries(const QMap<QString, QString> &secrets)
{
    ::rust::Vec<::komai::rust::SettingsStringMapEntry> entries;
    for (auto it = secrets.constBegin(); it != secrets.constEnd(); ++it) {
        entries.push_back({
          .key   = it.key().toStdString(),
          .value = it.value().toStdString(),
        });
    }
    return entries;
}

QMap<QString, QString>
fromRustStringMapEntries(const ::rust::Vec<::komai::rust::SettingsStringMapEntry> &entries)
{
    QMap<QString, QString> secrets;
    for (const auto &entry : entries) {
        secrets[QString::fromStdString(static_cast<std::string>(entry.key))] =
          QString::fromStdString(static_cast<std::string>(entry.value));
    }
    return secrets;
}

SecretsPersistenceContext
loadSecretsPersistenceContext(const QString &profileId)
{
    static QHash<QString, SecretsPersistenceContext> cache;

    auto it = cache.constFind(profileId);
    if (it != cache.constEnd())
        return *it;

    const auto config =
      ::komai::rust::settings_load_config_overview_for_profile(profileId.toStdString());

    SecretsPersistenceContext context{
      .usesFileSecretsProvider = config.uses_file_secrets_provider,
      .secureStoreKey = settings::storage::secureStoreKey(profileId, MatrixSdkSecureStoreKey),
    };

    cache.insert(profileId, context);
    return context;
}

QMap<QString, QString>
loadStoredMatrixSdkSecrets(const QString &profileId, const SecretsPersistenceContext &context)
{
    if (context.usesFileSecretsProvider) {
        return fromRustStringMapEntries(
          ::komai::rust::settings_load_matrix_sdk_secrets_for_profile(profileId.toStdString()));
    }

    const auto serializedSecrets = settings::storage::readSecureValue(context.secureStoreKey);
    if (!serializedSecrets.has_value() || serializedSecrets->isEmpty())
        return {};

    return settings::storage::decodeSecretsMap(*serializedSecrets);
}

void
saveStoredMatrixSdkSecrets(const SecretsPersistenceContext &context,
                           const QString &profileId,
                           const QMap<QString, QString> &secrets)
{
    if (context.usesFileSecretsProvider) {
        if (secrets.isEmpty()) {
            ::komai::rust::settings_remove_matrix_sdk_secrets_file_for_profile(
              profileId.toStdString());
            return;
        }

        ::komai::rust::settings_write_matrix_sdk_secrets_for_profile(
          profileId.toStdString(), toRustStringMapEntries(secrets), true);
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
    const auto secrets = loadStoredMatrixSdkSecrets(profileId, context);

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

    saveStoredMatrixSdkSecrets(context, profileId, storedSecrets);
}

void
clearPersistedMatrixSessionSecrets(const QString &profileId)
{
    savePersistedMatrixSessionSecrets(profileId, {});
}

} // namespace komai::matrix_backend
