// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <spdlog/logger.h>

#if __has_include(<qtkeychain/keychain.h>)
#include <qtkeychain/keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif

#include <memory>
#include <optional>

namespace settings::storage {

namespace {

const QString
keychainServiceName()
{
    return QCoreApplication::applicationName();
}

std::optional<bool>
forcedSecureBackendAvailabilityFromEnv()
{
    const auto forced = qgetenv("KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY").trimmed().toLower();
    if (forced.isNull() || forced.isEmpty())
        return std::nullopt;

    if (forced == "1" || forced == "true" || forced == "yes" || forced == "available")
        return true;

    if (forced == "0" || forced == "false" || forced == "no" || forced == "unavailable")
        return false;

    activeLoggers().ui->warn("Ignoring invalid KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY value '{}'; "
                             "expected available/unavailable or true/false",
                             forced.toStdString());
    return std::nullopt;
}

} // namespace

std::optional<QString>
readSecureValue(const QString &key)
{
    QEventLoop loop;
    auto job = std::make_unique<QKeychain::ReadPasswordJob>(keychainServiceName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(key);
    QObject::connect(job.get(), &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();

    if (job->error() == QKeychain::Error::NoError)
        return job->textData();

    if (job->error() != QKeychain::Error::EntryNotFound) {
        activeLoggers().db->warn("Failed to read secret '{}' from secure backend: {}",
                                 key.toStdString(),
                                 static_cast<int>(job->error()));
    }
    return std::nullopt;
}

void
writeSecureValue(const QString &key, const QString &value)
{
    QTimer::singleShot(0, QCoreApplication::instance(), [key, value] {
        auto *job = new QKeychain::WritePasswordJob(QCoreApplication::applicationName());
        job->setAutoDelete(true);
        job->setInsecureFallback(false);
        job->setKey(key);
        job->setTextData(value);
        QObject::connect(
          job, &QKeychain::WritePasswordJob::finished, job, [key](QKeychain::Job *j) {
              if (j->error() != QKeychain::Error::NoError) {
                  activeLoggers().db->warn("Failed to write secret '{}' to secure backend: {}",
                                           key.toStdString(),
                                           static_cast<int>(j->error()));
              }
          });
        job->start();
    });
}

void
deleteSecureValue(const QString &key)
{
    QTimer::singleShot(0, QCoreApplication::instance(), [key] {
        auto *job = new QKeychain::DeletePasswordJob(QCoreApplication::applicationName());
        job->setAutoDelete(true);
        job->setInsecureFallback(false);
        job->setKey(key);
        QObject::connect(
          job, &QKeychain::DeletePasswordJob::finished, job, [key](QKeychain::Job *j) {
              if (j->error() != QKeychain::Error::NoError &&
                  j->error() != QKeychain::Error::EntryNotFound) {
                  activeLoggers().db->warn("Failed to delete secret '{}' from secure backend: {}",
                                           key.toStdString(),
                                           static_cast<int>(j->error()));
              }
          });
        job->start();
    });
}

bool
isSecureBackendAvailable()
{
    if (const auto forced = forcedSecureBackendAvailabilityFromEnv(); forced.has_value()) {
        activeLoggers().ui->warn("Using forced secure-backend availability override "
                                 "(KOMAI_FORCE_SECRET_SERVICE_AVAILABILITY={}): {}",
                                 *forced ? "available" : "unavailable",
                                 *forced ? "secure backend enabled for provider selection"
                                         : "secure backend disabled for provider selection");
        return *forced;
    }

    if (!QCoreApplication::instance()) {
        activeLoggers().db->warn(
          "Secure backend availability probe requested before QCoreApplication exists");
        return false;
    }

    QEventLoop loop;
    auto job = std::make_unique<QKeychain::ReadPasswordJob>(keychainServiceName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(QStringLiteral("komai.secure_backend_probe"));
    QObject::connect(job.get(), &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();

    const auto error = job->error();
    if (error == QKeychain::Error::NoError || error == QKeychain::Error::EntryNotFound) {
        activeLoggers().db->info("Secure backend availability probe: available");
        return true;
    }

    activeLoggers().db->warn("Secure backend availability probe failed: {} ({})",
                             static_cast<int>(error),
                             job->errorString().toStdString());
    return false;
}

} // namespace settings::storage
