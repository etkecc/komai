// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile/ProfileSecrets.h"

#include "logging/Logging.h"
#include "profile/KeyringEnvironment.h"
#include "profile/ProfileId.h"
#include "settings/SettingsStorage.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace profile_secrets {

namespace {
constexpr std::string_view kCrossSigningMasterSecret      = "m.cross_signing.master";
constexpr std::string_view kCrossSigningSelfSigningSecret = "m.cross_signing.self_signing";
constexpr std::string_view kCrossSigningUserSigningSecret = "m.cross_signing.user_signing";
constexpr std::string_view kMegolmBackupV1Secret          = "m.megolm_backup.v1";
}

const std::array<CacheSecretDescriptor, 5> &
cacheSecretDescriptors() noexcept
{
    static const std::array<CacheSecretDescriptor, 5> descriptors{
      CacheSecretDescriptor{"pickle_secret", true},
      CacheSecretDescriptor{kCrossSigningMasterSecret, false},
      CacheSecretDescriptor{kCrossSigningSelfSigningSecret, false},
      CacheSecretDescriptor{kCrossSigningUserSigningSecret, false},
      CacheSecretDescriptor{kMegolmBackupV1Secret, false}};

    return descriptors;
}

const std::array<std::string_view, 1> &
settingsSecretNames() noexcept
{
    static const std::array<std::string_view, 1> names{"session.secrets"};
    return names;
}

bool
deleteProfileSecretValueBlocking(const QString &key)
{
    auto settings = UserSettings::instance();

    nhlog::ui()->info("Deleting profile secret '{}'", key.toStdString());

    if (settings->usesFileSecretsProvider()) {
        settings->removeSecret(key);
        nhlog::ui()->info("Deleted in-memory secret '{}' for insecure secret storage mode",
                          key.toStdString());
        return true;
    }

    constexpr int kDeleteAttempts = 3;

    for (int attempt = 1; attempt <= kDeleteAttempts; ++attempt) {
        const auto deleteResult = settings::storage::deleteSecureValueResultBlocking(key);
        if (!deleteResult.ok() && !deleteResult.missing()) {
            nhlog::ui()->warn("Failed to delete secret '{}' from secure backend on attempt {}: {}",
                              key.toStdString(),
                              attempt,
                              deleteResult.errorCode);
            return false;
        }

        const auto readResult = settings::storage::readSecureValueResult(key);
        if (readResult.failed()) {
            nhlog::ui()->warn(
              "Deleted secret '{}' from secure backend but could not verify removal",
              key.toStdString());
            return true;
        }
        if (readResult.missing()) {
            nhlog::ui()->info("Deleted secret '{}' from secure backend", key.toStdString());
            return true;
        }

        if (attempt < kDeleteAttempts) {
            nhlog::ui()->warn("Secret '{}' still present after deletion attempt {}."
                              " Retrying deletion.",
                              key.toStdString(),
                              attempt);
            continue;
        }

        nhlog::ui()->warn(
          "Failed to delete secret '{}' after {} attempts", key.toStdString(), attempt);
        return false;
    }

    return false;
}

bool
deleteEmptyProfileSecretValueBlocking(const QString &key)
{
    auto settings = UserSettings::instance();
    if (settings->usesFileSecretsProvider()) {
        nhlog::ui()->info(
          "Skipping secure-backend empty-secret cleanup for key '{}'; insecure mode",
          key.toStdString());
        settings->removeSecret(key);
        return true;
    }

    const auto firstRead = settings::storage::readSecureValueResult(key);
    if (firstRead.failed()) {
        nhlog::ui()->warn("Failed to verify cache secret '{}' for stale-empty cleanup",
                          key.toStdString());
        return false;
    }

    if (!firstRead.missing() && !firstRead.value.isEmpty()) {
        nhlog::ui()->debug(
          "Skipping deletion of cache secret '{}' because it now has non-empty value",
          key.toStdString());
        return true;
    }

    const auto secondRead = settings::storage::readSecureValueResult(key);
    if (secondRead.failed()) {
        nhlog::ui()->warn("Failed to re-verify cache secret '{}' for stale-empty cleanup",
                          key.toStdString());
        return false;
    }

    if (!secondRead.missing() && !secondRead.value.isEmpty()) {
        nhlog::ui()->debug(
          "Skipping deletion of cache secret '{}' because it was rewritten before cleanup",
          key.toStdString());
        return true;
    }

    return deleteProfileSecretValueBlocking(key);
}

bool
deleteAllProfileSecretsFromStoreBlocking(QStringView profile)
{
    auto settings       = UserSettings::instance();
    const auto provider = (settings && settings->usesFileSecretsProvider())
                            ? SecretStoreBackend::File
                            : SecretStoreBackend::SecureStore;
    return deleteAllProfileSecretsFromStoreBlocking(profile, provider);
}

bool
deleteAllProfileSecretsFromStoreBlocking(QStringView profile, SecretStoreBackend backend)
{
    if (backend == SecretStoreBackend::File)
        return true;

    return deleteSettingsProfileSecretsFromStoreBlocking(profile) &&
           deleteCacheProfileSecretsFromStoreBlocking(profile);
}

bool
deleteSettingsProfileSecretsFromStoreBlocking(QStringView profile)
{
    bool allRemoved = true;
    for (const auto &name : settingsSecretNames()) {
        const auto key = settingsSecretStoreKey(
          profile, QString::fromUtf8(name.data(), static_cast<int>(name.size())));
        if (!deleteProfileSecretValueBlocking(key))
            allRemoved = false;
    }

    return allRemoved;
}

bool
deleteCacheProfileSecretsFromStoreBlocking(QStringView profile)
{
    bool allRemoved = true;
    for (const auto &[name, internal] : cacheSecretDescriptors()) {
        if (!deleteProfileSecretValueBlocking(cacheSecretStoreKey(profile, name, internal)))
            allRemoved = false;
    }
    return allRemoved;
}

QString
normalizedProfileId(QStringView profile)
{
    return profile_id::normalized(profile);
}

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName)
{
    return keyring_environment::prefix() + normalizedProfileId(profile) +
           QStringLiteral(".settings.") + keyName.toString();
}

QString
cacheSecretStoreKey(QStringView profile, std::string_view keyName, bool internal)
{
    return keyring_environment::prefix() + normalizedProfileId(profile) + QStringLiteral(".") +
           (internal ? QStringLiteral("local_crypto.") : QStringLiteral("matrix.")) +
           QString::fromUtf8(keyName.data(), static_cast<int>(keyName.size()));
}

} // namespace profile_secrets
