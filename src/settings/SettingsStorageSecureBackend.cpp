// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>

#include <spdlog/logger.h>

#if __has_include(<keychain.h>)
#include <keychain.h>
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

} // namespace settings::storage
