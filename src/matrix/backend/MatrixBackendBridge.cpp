// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendBridge.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <QThread>

#include <optional>
#include <type_traits>
#include <utility>

#include "logging/Logging.h"
#include "matrix/backend/MatrixSessionSecrets.h"
#include "profile/Paths.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

namespace {

QString
toQString(rust::Str value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

template<typename Func>
auto
invokeOnAppThread(Func &&func)
{
    using Result = std::invoke_result_t<Func>;

    auto *app = QCoreApplication::instance();
    if (!app || QThread::currentThread() == app->thread()) {
        if constexpr (std::is_void_v<Result>) {
            func();
            return;
        } else {
            return func();
        }
    }

    if constexpr (std::is_void_v<Result>) {
        const bool invoked =
          QMetaObject::invokeMethod(app, [&func]() { func(); }, Qt::BlockingQueuedConnection);
        if (!invoked) {
            if (auto logger = nhlog::rust(); logger) {
                logger->warn("Failed to dispatch matrix backend bridge task to app thread; "
                             "running inline on current thread");
            }
            func();
        }
    } else {
        std::optional<Result> result;
        const bool invoked = QMetaObject::invokeMethod(
          app, [&func, &result]() { result.emplace(func()); }, Qt::BlockingQueuedConnection);
        if (!invoked) {
            if (auto logger = nhlog::rust(); logger) {
                logger->warn("Failed to dispatch matrix backend bridge task to app thread; "
                             "running inline on current thread");
            }
            result.emplace(func());
        }
        return std::move(*result);
    }
}

template<typename Func>
void
postToAppThread(Func &&func)
{
    auto callback = std::forward<Func>(func);
    auto *app     = QCoreApplication::instance();
    if (!app || QThread::currentThread() == app->thread()) {
        callback();
        return;
    }

    const bool invoked = QMetaObject::invokeMethod(app, callback, Qt::QueuedConnection);
    if (!invoked) {
        if (auto logger = nhlog::rust(); logger) {
            logger->warn(
              "Failed to queue matrix backend bridge notification on app thread; running inline");
        }
        callback();
    }
}

} // namespace

namespace komai::rust_bridge {

rust::String
matrix_profile_data_root(rust::Str profile_id)
{
    return rust::String(app_paths::data::profileDirectory(toQString(profile_id)).toStdString());
}

rust::String
matrix_profile_cache_root(rust::Str profile_id)
{
    return rust::String(app_paths::cache::profileDirectory(toQString(profile_id)).toStdString());
}

rust::String
matrix_store_passphrase(rust::Str profile_id)
{
    const auto secrets = invokeOnAppThread([&profile_id]() {
        return matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    });
    return rust::String(secrets.storePassphrase.toStdString());
}

rust::String
matrix_homeserver_url(rust::Str profile_id)
{
    const auto secrets = invokeOnAppThread([&profile_id]() {
        return matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    });
    return rust::String(secrets.homeserverUrl.toStdString());
}

rust::String
matrix_serialized_session(rust::Str profile_id)
{
    const auto secrets = invokeOnAppThread([&profile_id]() {
        return matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    });
    return rust::String(secrets.serializedSession.toStdString());
}

void
matrix_save_session_secrets(rust::Str profile_id,
                            rust::Str store_passphrase,
                            rust::Str homeserver_url,
                            rust::Str serialized_session)
{
    invokeOnAppThread([&profile_id, &store_passphrase, &homeserver_url, &serialized_session]() {
        matrix_backend::savePersistedMatrixSessionSecrets(
          toQString(profile_id),
          {
            .storePassphrase   = toQString(store_passphrase),
            .homeserverUrl     = toQString(homeserver_url),
            .serializedSession = toQString(serialized_session),
          });
    });
}

void
matrix_clear_session_secrets(rust::Str profile_id)
{
    invokeOnAppThread([&profile_id]() {
        matrix_backend::clearPersistedMatrixSessionSecrets(toQString(profile_id));
    });
}

void
matrix_log_event(rust::Str level,
                 rust::Str target,
                 rust::Str module_path,
                 rust::Str file,
                 std::uint32_t line,
                 rust::Str message)
{
    auto logger = nhlog::rust();
    if (!logger)
        return;

    const auto levelString      = toQString(level);
    const auto targetString     = toQString(target);
    const auto modulePathString = toQString(module_path);
    const auto fileString       = toQString(file);
    const auto messageString    = toQString(message);

    QString formatted = messageString;
    if (!targetString.isEmpty())
        formatted.prepend(QStringLiteral("[%1] ").arg(targetString));

    QStringList metadata;
    if (!modulePathString.isEmpty())
        metadata.push_back(modulePathString);
    if (!fileString.isEmpty() && line > 0)
        metadata.push_back(QStringLiteral("%1:%2").arg(fileString).arg(line));
    else if (!fileString.isEmpty())
        metadata.push_back(fileString);

    if (!metadata.isEmpty())
        formatted += QStringLiteral(" (%1)").arg(metadata.join(QStringLiteral(", ")));

    const auto formattedStd = formatted.toStdString();

    if (levelString == QStringLiteral("trace"))
        logger->trace("{}", formattedStd);
    else if (levelString == QStringLiteral("debug"))
        logger->debug("{}", formattedStd);
    else if (levelString == QStringLiteral("warn"))
        logger->warn("{}", formattedStd);
    else if (levelString == QStringLiteral("error"))
        logger->error("{}", formattedStd);
    else
        logger->info("{}", formattedStd);
}

void
matrix_notify_room_list_snapshot_updated(std::uint64_t handle_id)
{
    postToAppThread([handle_id]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendRoomListSnapshotUpdated(handle_id);
    });
}

void
matrix_notify_room_timeline_snapshot_updated(std::uint64_t handle_id, rust::Str room_id)
{
    const auto roomId = toQString(room_id);
    postToAppThread([handle_id, roomId]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendRoomTimelineSnapshotUpdated(handle_id, roomId);
    });
}

} // namespace komai::rust_bridge
