// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QString>

#include "settings/SettingsPersistence.h"

#include "Logging.h"
#include "Paths.h"
#include "ProfileSecrets.h"
#include "settings/SettingsStorage.h"
#include "settings/YamlSettings.h"

namespace settings::persistence {
namespace {

bool
isFileProvider(bool runWithoutSecureSecretsService)
{
    if (runWithoutSecureSecretsService)
        return true;
    return false;
}

void
storeInternalSessionMetadata(QMap<QString, QString> &secrets,
                             const QString &userId,
                             const QString &deviceId,
                             const QString &homeserver)
{
    constexpr auto sessionUserIdKey     = "__session.user_id";
    constexpr auto sessionDeviceIdKey   = "__session.device_id";
    constexpr auto sessionHomeserverKey = "__session.homeserver";

    if (userId.isEmpty())
        secrets.remove(sessionUserIdKey);
    else
        secrets[sessionUserIdKey] = userId;

    if (deviceId.isEmpty())
        secrets.remove(sessionDeviceIdKey);
    else
        secrets[sessionDeviceIdKey] = deviceId;

    if (homeserver.isEmpty())
        secrets.remove(sessionHomeserverKey);
    else
        secrets[sessionHomeserverKey] = homeserver;
}

void
extractInternalSessionMetadata(SecretsPayload &payload)
{
    constexpr auto sessionUserIdKey     = "__session.user_id";
    constexpr auto sessionDeviceIdKey   = "__session.device_id";
    constexpr auto sessionHomeserverKey = "__session.homeserver";

    payload.sessionUserId     = payload.secrets.value(sessionUserIdKey);
    payload.sessionDeviceId   = payload.secrets.value(sessionDeviceIdKey);
    payload.sessionHomeserver = payload.secrets.value(sessionHomeserverKey);

    payload.secrets.remove(sessionUserIdKey);
    payload.secrets.remove(sessionDeviceIdKey);
    payload.secrets.remove(sessionHomeserverKey);
}

} // namespace

staged_load_plan::SecretsProvider
providerFromConfig(const YAML::Node &configRoot, bool runWithoutSecureSecretsService)
{
    const auto configProvider     = staged_load_plan::providerFromConfig(configRoot);
    const auto forcedFileProvider = isFileProvider(runWithoutSecureSecretsService);
    if (forcedFileProvider)
        return staged_load_plan::SecretsProvider::File;

    return configProvider;
}

SecretsPayload
loadProfileSecrets(const QString &profile,
                   bool runWithoutSecureSecretsService,
                   const QString &secretsFilePath)
{
    SecretsPayload payload;
    bool hasEmptySecureSecrets   = false;
    const auto normalizedProfile = app_paths::normalizedProfileId(profile);

    if (isFileProvider(runWithoutSecureSecretsService)) {
        const auto secretsRoot = settings::storage::loadYamlFile(secretsFilePath, "secrets");
        payload.accessToken =
          yaml_settings::readString(secretsRoot, SettingKey::SecretsFileAuthAccessToken, QString());
        payload.secrets = yaml_settings::readStringMap(secretsRoot, SettingKey::SecretsFileMap);
        extractInternalSessionMetadata(payload);

        nhlog::ui()->info("Loaded file-backed secrets (has_access_token={}, secrets_count={})",
                          !payload.accessToken.trimmed().isEmpty(),
                          payload.secrets.size());
        return payload;
    }

    const auto accessTokenStoreKey =
      settings::storage::secureStoreKey(profile, SecureStoreAccessTokenKey);
    const auto secureAccessToken = settings::storage::readSecureValue(accessTokenStoreKey);
    if (secureAccessToken && secureAccessToken->isEmpty()) {
        nhlog::ui()->warn("Secure backend access token was empty; removing stale session auth "
                          "secret for profile '{}'",
                          normalizedProfile.toStdString());
        const auto staleAccessTokenDeleted =
          profile_secrets::deleteProfileSecretValueBlocking(accessTokenStoreKey);
        if (!staleAccessTokenDeleted) {
            nhlog::ui()->warn(
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
        nhlog::ui()->warn("Secure backend secrets payload was empty; removing stale secret storage "
                          "for profile '{}'",
                          normalizedProfile.toStdString());
        const auto staleSecretsDeleted =
          profile_secrets::deleteProfileSecretValueBlocking(secretsStoreKey);
        if (!staleSecretsDeleted) {
            nhlog::ui()->warn(
              "Failed to remove stale secure backend session secrets for profile '{}'",
              normalizedProfile.toStdString());
        }
        hasEmptySecureSecrets = true;
    } else {
        payload.secrets = serializedSecrets
                            ? settings::storage::decodeSecretsMap(*serializedSecrets)
                            : QMap<QString, QString>{};
        extractInternalSessionMetadata(payload);

        bool sessionSecretsPruned = false;
        for (auto it = payload.secrets.begin(); it != payload.secrets.end();) {
            if (it.value().isEmpty()) {
                nhlog::ui()->warn("Pruning empty secure secret entry '{}' for profile '{}'",
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
                    nhlog::ui()->warn(
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

    if (hasEmptySecureSecrets)
        nhlog::ui()->warn("Found stale/empty secure backend values for profile '{}'",
                          normalizedProfile.toStdString());

    nhlog::ui()->info("Loaded secure-backend secrets (has_access_token={}, secrets_count={})",
                      !payload.accessToken.trimmed().isEmpty(),
                      payload.secrets.size());

    payload.hadStaleValues = hasEmptySecureSecrets;
    return payload;
}

void
saveProfileSecrets(const QString &profile,
                   bool runWithoutSecureSecretsService,
                   const QString &secretsFilePath,
                   const QString &accessToken,
                   const QMap<QString, QString> &secrets,
                   const QString &sessionUserId,
                   const QString &sessionDeviceId,
                   const QString &sessionHomeserver)
{
    auto secretsWithSessionMetadata = secrets;
    storeInternalSessionMetadata(
      secretsWithSessionMetadata, sessionUserId, sessionDeviceId, sessionHomeserver);

    if (isFileProvider(runWithoutSecureSecretsService)) {
        YAML::Node root(YAML::NodeType::Map);
        yaml_settings::setNode(
          root, SettingKey::SecretsFileAuthAccessToken, accessToken.toStdString());
        yaml_settings::writeStringMap(root, SettingKey::SecretsFileMap, secretsWithSessionMetadata);

        if (settings::storage::writeYamlFile(secretsFilePath, root, true)) {
            nhlog::ui()->debug("Saved secrets to: {}", secretsFilePath.toStdString());
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

    if (settings::storage::pathExists(secretsFilePath) && !settings::storage::removePath(secretsFilePath))
        nhlog::ui()->warn("Failed to remove stale secrets file: {}", secretsFilePath.toStdString());
}

bool
clearProfileSecrets(const QString &profile,
                    bool runWithoutSecureSecretsService,
                    const QString &secretsFilePath)
{
    if (runWithoutSecureSecretsService) {
        const auto normalizedProfile = app_paths::normalizedProfileId(profile);
        if (settings::storage::pathExists(secretsFilePath) &&
            !settings::storage::removePath(secretsFilePath)) {
            nhlog::ui()->warn("Failed to remove stale secrets file: {}",
                              secretsFilePath.toStdString());
            return false;
        }
        nhlog::ui()->info("Cleared file-backed secrets for profile '{}'",
                          normalizedProfile.toStdString());
        return true;
    }

    const auto normalizedProfile = app_paths::normalizedProfileId(profile);
    const auto allSecretsDeleted =
      profile_secrets::deleteAllProfileSecretsFromStoreBlocking(profile);
    if (!allSecretsDeleted) {
        nhlog::ui()->warn("Failed to delete all profile secrets during logout for profile '{}'",
                          normalizedProfile.toStdString());
    }

    if (settings::storage::pathExists(secretsFilePath) &&
        !settings::storage::removePath(secretsFilePath)) {
        nhlog::ui()->warn("Failed to remove stale secrets file: {}", secretsFilePath.toStdString());
        return false;
    }

    return allSecretsDeleted;
}

} // namespace settings::persistence
