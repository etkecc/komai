// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProfileSecrets.h"

#include <QCoreApplication>
#include <QEventLoop>

#include <QCryptographicHash>
#include <optional>

#if __has_include(<keychain.h>)
#include <keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif

#include <mtx/secret_storage.hpp>

#include "Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace profile_secrets {

const std::array<CacheSecretDescriptor, 5> &
cacheSecretDescriptors() noexcept
{
    static const std::array<CacheSecretDescriptor, 5> descriptors{
      CacheSecretDescriptor{"pickle_secret", true},
      CacheSecretDescriptor{mtx::secret_storage::secrets::cross_signing_master, false},
      CacheSecretDescriptor{mtx::secret_storage::secrets::cross_signing_self_signing, false},
      CacheSecretDescriptor{mtx::secret_storage::secrets::cross_signing_user_signing, false},
      CacheSecretDescriptor{mtx::secret_storage::secrets::megolm_backup_v1, false}};

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

    auto deleteJobStatus = [&](const QString &target) -> std::optional<QKeychain::Error> {
        std::optional<QKeychain::Error> status;
        QEventLoop loop;
        auto *job = new QKeychain::DeletePasswordJob(QCoreApplication::applicationName());
        job->setAutoDelete(true);
        job->setInsecureFallback(false);
        job->setKey(target);

        QObject::connect(job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
        QObject::connect(job, &QKeychain::Job::finished, job, [&status](QKeychain::Job *j) {
            status = j->error();
        });
        job->start();
        loop.exec();
        return status;
    };

    auto readJobStatus = [&](const QString &target) -> std::optional<bool> {
        std::optional<bool> status;
        QEventLoop loop;
        auto *job = new QKeychain::ReadPasswordJob(QCoreApplication::applicationName());
        job->setAutoDelete(true);
        job->setInsecureFallback(false);
        job->setKey(target);

        QObject::connect(job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
        QObject::connect(job, &QKeychain::Job::finished, job, [&status](QKeychain::Job *j) {
            if (j->error() == QKeychain::Error::NoError)
                status = true;
            else if (j->error() == QKeychain::Error::EntryNotFound)
                status = false;
            else
                status.reset();
        });
        job->start();
        loop.exec();
        return status;
    };

    for (int attempt = 1; attempt <= kDeleteAttempts; ++attempt) {
        const auto error = deleteJobStatus(key);
        if (!error) {
            nhlog::ui()->warn("Timed out while deleting secret '{}' from secure backend",
                              key.toStdString());
            return false;
        }

        if (*error != QKeychain::Error::NoError && *error != QKeychain::Error::EntryNotFound) {
            nhlog::ui()->warn("Failed to delete secret '{}' from secure backend on attempt {}: {}",
                              key.toStdString(),
                              attempt,
                              static_cast<int>(*error));
            return false;
        }

        const auto exists = readJobStatus(key);
        if (!exists) {
            nhlog::ui()->warn(
              "Deleted secret '{}' from secure backend but could not verify removal",
              key.toStdString());
            return true;
        }
        if (!*exists) {
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

    auto readJobStatus = [&](const QString &target) -> std::optional<bool> {
        std::optional<bool> status;
        QEventLoop loop;
        auto *job = new QKeychain::ReadPasswordJob(QCoreApplication::applicationName());
        job->setAutoDelete(true);
        job->setInsecureFallback(false);
        job->setKey(target);

        QObject::connect(job, &QKeychain::Job::finished, &loop, &QEventLoop::quit);
        QObject::connect(job, &QKeychain::Job::finished, job, [&status](QKeychain::Job *j) {
            if (j->error() == QKeychain::Error::NoError)
                status = static_cast<QKeychain::ReadPasswordJob *>(j)->textData().isEmpty();
            else if (j->error() == QKeychain::Error::EntryNotFound)
                status = true;
            else
                status.reset();
        });
        job->start();
        loop.exec();
        return status;
    };

    const auto isEmpty = readJobStatus(key);
    if (!isEmpty) {
        nhlog::ui()->warn("Failed to verify cache secret '{}' for stale-empty cleanup",
                          key.toStdString());
        return false;
    }

    if (!*isEmpty) {
        nhlog::ui()->debug(
          "Skipping deletion of cache secret '{}' because it now has non-empty value",
          key.toStdString());
        return true;
    }

    const auto isEmptyAfterRead = readJobStatus(key);
    if (!isEmptyAfterRead) {
        nhlog::ui()->warn("Failed to re-verify cache secret '{}' for stale-empty cleanup",
                          key.toStdString());
        return false;
    }

    if (!*isEmptyAfterRead) {
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
    if (profile.isEmpty() || profile == u"default")
        return QStringLiteral("default");
    return profile.toString();
}

QString
profileHashHex(QStringView profile)
{
    return QString::fromLatin1(
      QCryptographicHash::hash(normalizedProfileId(profile).toUtf8(), QCryptographicHash::Sha256)
        .toHex());
}

QString
settingsSecretStoreKey(QStringView profile, QStringView keyName)
{
    return QStringLiteral("komai.") + profileHashHex(profile) + QStringLiteral(".settings.") +
           keyName.toString();
}

QString
cacheSecretStoreKey(QStringView profile, std::string_view keyName, bool internal)
{
    return QStringLiteral("komai.") + profileHashHex(profile) + QStringLiteral(".") +
           (internal ? QStringLiteral("local_crypto.") : QStringLiteral("matrix.")) +
           QString::fromUtf8(keyName.data(), static_cast<int>(keyName.size()));
}

} // namespace profile_secrets
