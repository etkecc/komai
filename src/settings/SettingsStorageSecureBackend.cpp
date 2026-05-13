// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "SettingsStorage.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include "logging/Logging.h"

#if __has_include(<qtkeychain/keychain.h>)
#include <qtkeychain/keychain.h>
#else
#include <qt6keychain/keychain.h>
#endif

#include <memory>
#include <optional>
#include <utility>

namespace settings::storage {

namespace {

class SecureBackendDispatcher final : public QObject
{};

struct SecureBackendThreadState
{
    QThread *thread                = nullptr;
    SecureBackendDispatcher *entry = nullptr;
};

const QString
keychainServiceName()
{
    return QCoreApplication::applicationName();
}

SecureBackendThreadState &
secureBackendThreadState()
{
    static auto *state = [] {
        auto *createdState   = new SecureBackendThreadState;
        createdState->thread = new QThread;
        createdState->thread->setObjectName(QStringLiteral("KomaiSecureBackendKeychain"));
        createdState->entry = new SecureBackendDispatcher;
        createdState->entry->moveToThread(createdState->thread);

        if (auto *app = QCoreApplication::instance()) {
            QObject::connect(
              app, &QCoreApplication::aboutToQuit, createdState->thread, &QThread::quit);
        }

        createdState->thread->start();
        return createdState;
    }();

    return *state;
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

SecureBackendJobResult
failedSecureBackendResult(const QString &message)
{
    return {
      .status      = SecureBackendJobStatus::Error,
      .value       = {},
      .errorCode   = -1,
      .errorString = message,
    };
}

SecureBackendJobResult
resultFromKeychainJob(const QKeychain::Job &job, QString value = {})
{
    const auto error = job.error();
    if (error == QKeychain::Error::NoError) {
        return {
          .status      = SecureBackendJobStatus::Success,
          .value       = std::move(value),
          .errorCode   = static_cast<int>(error),
          .errorString = job.errorString(),
        };
    }

    if (error == QKeychain::Error::EntryNotFound) {
        return {
          .status      = SecureBackendJobStatus::EntryNotFound,
          .value       = {},
          .errorCode   = static_cast<int>(error),
          .errorString = job.errorString(),
        };
    }

    return {
      .status      = SecureBackendJobStatus::Error,
      .value       = {},
      .errorCode   = static_cast<int>(error),
      .errorString = job.errorString(),
    };
}

template<typename Func>
SecureBackendJobResult
invokeSecureBackendBlocking(Func &&func)
{
    auto *app = QCoreApplication::instance();
    if (!app) {
        activeLoggers().db->warn(
          "Secure backend operation requested before QCoreApplication exists");
        return failedSecureBackendResult(QStringLiteral("No QCoreApplication instance"));
    }

#ifdef Q_OS_MACOS
    // qtkeychain's macOS backend dispatches Job::finished completions via the main GCD queue, so hopping to our worker via BlockingQueuedConnection from the main thread deadlocks: main blocks waiting for the worker, the worker spins inside performRead*'s nested QEventLoop waiting for finished, and the completion that would fire finished is itself queued on the (now blocked) main thread.
    // Running on the caller thread lets the nested QEventLoop pump the dispatch queue directly and the call returns normally.
    return std::forward<Func>(func)();
#else
    auto &state    = secureBackendThreadState();
    auto operation = std::forward<Func>(func);
    SecureBackendJobResult result;

    if (QThread::currentThread() == state.entry->thread())
        return operation();

    const bool invoked = QMetaObject::invokeMethod(
      state.entry,
      [&result, operation = std::move(operation)]() mutable { result = operation(); },
      Qt::BlockingQueuedConnection);
    if (!invoked) {
        activeLoggers().db->warn("Failed to invoke secure backend blocking operation");
        return failedSecureBackendResult(QStringLiteral("Failed to invoke secure backend task"));
    }

    return result;
#endif
}

template<typename Func>
bool
enqueueSecureBackendAsync(Func &&func)
{
    auto *app = QCoreApplication::instance();
    if (!app) {
        activeLoggers().db->warn(
          "Secure backend async operation requested before QCoreApplication exists");
        return false;
    }

    auto &state    = secureBackendThreadState();
    auto operation = std::forward<Func>(func);

    if (QThread::currentThread() == state.entry->thread()) {
        operation();
        return true;
    }

    const bool invoked = QMetaObject::invokeMethod(
      state.entry,
      [operation = std::move(operation)]() mutable { operation(); },
      Qt::QueuedConnection);
    if (!invoked)
        activeLoggers().db->warn("Failed to queue secure backend async operation");
    return invoked;
}

void
deliverSecureBackendResult(QPointer<QObject> receiver,
                           std::function<void(const SecureBackendJobResult &)> callback,
                           const SecureBackendJobResult &result)
{
    if (!callback)
        return;

    if (!receiver) {
        callback(result);
        return;
    }

    if (QThread::currentThread() == receiver->thread()) {
        callback(result);
        return;
    }

    const bool invoked = QMetaObject::invokeMethod(
      receiver,
      [receiver, callback = std::move(callback), result]() mutable {
          if (receiver)
              callback(result);
      },
      Qt::QueuedConnection);
    if (!invoked)
        activeLoggers().db->warn("Failed to deliver secure backend callback result");
}

SecureBackendJobResult
performReadSecureValue(const QString &key)
{
    QEventLoop loop;
    auto job = std::make_unique<QKeychain::ReadPasswordJob>(keychainServiceName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(key);
    QObject::connect(job.get(), &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();

    return resultFromKeychainJob(*job, job->textData());
}

SecureBackendJobResult
performWriteSecureValue(const QString &key, const QString &value)
{
    QEventLoop loop;
    auto job = std::make_unique<QKeychain::WritePasswordJob>(keychainServiceName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(key);
    job->setTextData(value);
    QObject::connect(job.get(), &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();

    return resultFromKeychainJob(*job);
}

SecureBackendJobResult
performDeleteSecureValue(const QString &key)
{
    QEventLoop loop;
    auto job = std::make_unique<QKeychain::DeletePasswordJob>(keychainServiceName());
    job->setAutoDelete(false);
    job->setInsecureFallback(false);
    job->setKey(key);
    QObject::connect(job.get(), &QKeychain::Job::finished, &loop, &QEventLoop::quit);
    job->start();
    loop.exec();

    return resultFromKeychainJob(*job);
}

} // namespace

SecureBackendJobResult
readSecureValueResult(const QString &key)
{
    return invokeSecureBackendBlocking([key] { return performReadSecureValue(key); });
}

std::optional<QString>
readSecureValue(const QString &key)
{
    const auto result = readSecureValueResult(key);
    if (result.ok())
        return result.value;

    if (result.failed()) {
        activeLoggers().db->warn("Failed to read secret '{}' from secure backend: {}",
                                 key.toStdString(),
                                 result.errorCode);
    }
    return std::nullopt;
}

void
readSecureValueAsync(const QString &key,
                     QObject *receiver,
                     std::function<void(const SecureBackendJobResult &)> callback)
{
    const QPointer<QObject> safeReceiver(receiver);
    const bool queued =
      enqueueSecureBackendAsync([key, safeReceiver, callback = std::move(callback)]() mutable {
          auto *job = new QKeychain::ReadPasswordJob(keychainServiceName());
          job->setAutoDelete(true);
          job->setInsecureFallback(false);
          job->setKey(key);
          QObject::connect(
            job,
            &QKeychain::Job::finished,
            job,
            [safeReceiver, callback = std::move(callback)](QKeychain::Job *j) mutable {
                const auto result = resultFromKeychainJob(
                  *j, static_cast<QKeychain::ReadPasswordJob *>(j)->textData());
                deliverSecureBackendResult(safeReceiver, std::move(callback), result);
            });
          job->start();
      });
    if (!queued)
        deliverSecureBackendResult(
          safeReceiver,
          std::move(callback),
          failedSecureBackendResult(QStringLiteral("Failed to queue secure backend read task")));
}

SecureBackendJobResult
writeSecureValueResultBlocking(const QString &key, const QString &value)
{
    return invokeSecureBackendBlocking(
      [key, value] { return performWriteSecureValue(key, value); });
}

bool
writeSecureValueBlocking(const QString &key, const QString &value)
{
    const auto result = writeSecureValueResultBlocking(key, value);
    if (result.ok())
        return true;

    activeLoggers().db->warn(
      "Failed to write secret '{}' to secure backend: {}", key.toStdString(), result.errorCode);
    return false;
}

void
writeSecureValue(const QString &key, const QString &value)
{
    writeSecureValueAsync(key, value, nullptr, [key](const SecureBackendJobResult &result) {
        if (!result.ok()) {
            activeLoggers().db->warn("Failed to write secret '{}' to secure backend: {}",
                                     key.toStdString(),
                                     result.errorCode);
        }
    });
}

void
writeSecureValueAsync(const QString &key,
                      const QString &value,
                      QObject *receiver,
                      std::function<void(const SecureBackendJobResult &)> callback)
{
    const QPointer<QObject> safeReceiver(receiver);
    const bool queued = enqueueSecureBackendAsync(
      [key, value, safeReceiver, callback = std::move(callback)]() mutable {
          auto *job = new QKeychain::WritePasswordJob(keychainServiceName());
          job->setAutoDelete(true);
          job->setInsecureFallback(false);
          job->setKey(key);
          job->setTextData(value);
          QObject::connect(
            job,
            &QKeychain::Job::finished,
            job,
            [safeReceiver, callback = std::move(callback)](QKeychain::Job *j) mutable {
                const auto result = resultFromKeychainJob(*j);
                deliverSecureBackendResult(safeReceiver, std::move(callback), result);
            });
          job->start();
      });
    if (!queued)
        deliverSecureBackendResult(
          safeReceiver,
          std::move(callback),
          failedSecureBackendResult(QStringLiteral("Failed to queue secure backend write task")));
}

SecureBackendJobResult
deleteSecureValueResultBlocking(const QString &key)
{
    return invokeSecureBackendBlocking([key] { return performDeleteSecureValue(key); });
}

bool
deleteSecureValueBlocking(const QString &key)
{
    const auto result = deleteSecureValueResultBlocking(key);
    if (result.ok() || result.missing()) {
        return true;
    }

    activeLoggers().db->warn(
      "Failed to delete secret '{}' from secure backend: {}", key.toStdString(), result.errorCode);
    return false;
}

void
deleteSecureValue(const QString &key)
{
    deleteSecureValueAsync(key, nullptr, [key](const SecureBackendJobResult &result) {
        if (result.failed()) {
            activeLoggers().db->warn("Failed to delete secret '{}' from secure backend: {}",
                                     key.toStdString(),
                                     result.errorCode);
        }
    });
}

void
deleteSecureValueAsync(const QString &key,
                       QObject *receiver,
                       std::function<void(const SecureBackendJobResult &)> callback)
{
    const QPointer<QObject> safeReceiver(receiver);
    const bool queued =
      enqueueSecureBackendAsync([key, safeReceiver, callback = std::move(callback)]() mutable {
          auto *job = new QKeychain::DeletePasswordJob(keychainServiceName());
          job->setAutoDelete(true);
          job->setInsecureFallback(false);
          job->setKey(key);
          QObject::connect(
            job,
            &QKeychain::Job::finished,
            job,
            [safeReceiver, callback = std::move(callback)](QKeychain::Job *j) mutable {
                const auto result = resultFromKeychainJob(*j);
                deliverSecureBackendResult(safeReceiver, std::move(callback), result);
            });
          job->start();
      });
    if (!queued)
        deliverSecureBackendResult(
          safeReceiver,
          std::move(callback),
          failedSecureBackendResult(QStringLiteral("Failed to queue secure backend delete task")));
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

    const auto result = readSecureValueResult(QStringLiteral("komai.secure_backend_probe"));
    if (result.ok() || result.missing()) {
        activeLoggers().db->info("Secure backend availability probe: available");
        return true;
    }

    activeLoggers().db->warn("Secure backend availability probe failed: {} ({})",
                             result.errorCode,
                             result.errorString.toStdString());
    return false;
}

} // namespace settings::storage
