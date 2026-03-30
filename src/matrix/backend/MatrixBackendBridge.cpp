// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendBridge.h"
#include "komai-rust-cxxbridge/lib.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
#include <QStringList>
#include <QThread>

#include <utility>

#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixSessionSecrets.h"
#include "profile/Paths.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"

namespace {

QString
toQString(::rust::Str value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

komai::MatrixRoomSummary
fromRustRoomSummary(const ::komai::rust::MatrixRoomSummary &room)
{
    QVector<QString> tags;
    tags.reserve(static_cast<int>(room.tags.size()));
    for (const auto &value : room.tags)
        tags.push_back(QString::fromStdString(std::string(value)));

    QVector<QString> parentSpaceRoomIds;
    parentSpaceRoomIds.reserve(static_cast<int>(room.parent_space_room_ids.size()));
    for (const auto &value : room.parent_space_room_ids)
        parentSpaceRoomIds.push_back(QString::fromStdString(std::string(value)));

    return komai::MatrixRoomSummary{
      .roomId        = QString::fromStdString(std::string(room.room_id)),
      .latestEventId = QString::fromStdString(std::string(room.latest_event_id)),
      .displayName   = QString::fromStdString(std::string(room.display_name)),
      .avatarUrl =
        komai::matrix::normalizeMxcUri(QString::fromStdString(std::string(room.avatar_url))),
      .topic                 = QString::fromStdString(std::string(room.topic)),
      .lastMessage           = QString::fromStdString(std::string(room.last_message)),
      .lastMessageKind       = QString::fromStdString(std::string(room.last_message_kind)),
      .tags                  = std::move(tags),
      .parentSpaceRoomIds    = std::move(parentSpaceRoomIds),
      .directChatOtherUserId = QString::fromStdString(std::string(room.direct_chat_other_user_id)),
      .isInvite              = room.is_invite,
      .isSpace               = room.is_space,
      .isDirect              = room.is_direct,
      .isBotRoom             = room.is_bot_room,
      .isEncrypted           = room.is_encrypted,
      .isPublic              = room.is_public,
      .memberCount           = room.member_count,
      .unreadMessages        = room.unread_message_count,
      .notificationCount     = room.notification_count,
      .highlightCount        = room.highlight_count,
      .timestamp             = room.timestamp,
    };
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

::rust::String
matrix_profile_data_root(::rust::Str profile_id)
{
    return ::rust::String(app_paths::data::profileDirectory(toQString(profile_id)).toStdString());
}

::rust::String
matrix_profile_cache_root(::rust::Str profile_id)
{
    return ::rust::String(app_paths::cache::profileDirectory(toQString(profile_id)).toStdString());
}

::rust::String
matrix_store_passphrase(::rust::Str profile_id)
{
    const auto secrets = matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    return ::rust::String(secrets.storePassphrase.toStdString());
}

::rust::String
matrix_homeserver_url(::rust::Str profile_id)
{
    const auto secrets = matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    return ::rust::String(secrets.homeserverUrl.toStdString());
}

::rust::String
matrix_serialized_session(::rust::Str profile_id)
{
    const auto secrets = matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    return ::rust::String(secrets.serializedSession.toStdString());
}

void
matrix_save_session_secrets(::rust::Str profile_id,
                            ::rust::Str store_passphrase,
                            ::rust::Str homeserver_url,
                            ::rust::Str serialized_session)
{
    // Session refresh callbacks may run while the UI thread is already inside a
    // synchronous runtime().block_on(...) call. Do not bounce persistence back
    // to the app thread here or we can deadlock the refresh path.
    matrix_backend::savePersistedMatrixSessionSecrets(
      toQString(profile_id),
      {
        .storePassphrase   = toQString(store_passphrase),
        .homeserverUrl     = toQString(homeserver_url),
        .serializedSession = toQString(serialized_session),
      });
}

void
matrix_clear_session_secrets(::rust::Str profile_id)
{
    matrix_backend::clearPersistedMatrixSessionSecrets(toQString(profile_id));
}

void
matrix_log_event(::rust::Str level,
                 ::rust::Str target,
                 ::rust::Str module_path,
                 ::rust::Str file,
                 std::uint32_t line,
                 ::rust::Str message)
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
matrix_notify_room_list_snapshot_updated(std::uint64_t handle_id,
                                         ::rust::Vec<::komai::rust::MatrixRoomSummary> room_list)
{
    QVector<komai::MatrixRoomSummary> snapshot;
    snapshot.reserve(static_cast<int>(room_list.size()));
    for (const auto &room : room_list)
        snapshot.push_back(fromRustRoomSummary(room));

    postToAppThread([handle_id, snapshot = std::move(snapshot)]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        komai::MatrixBackendRuntimeService::cacheRoomListSnapshot(handle_id, snapshot);
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendRoomListSnapshotUpdated(handle_id);
    });
}

void
matrix_notify_initial_sync_ready(std::uint64_t handle_id)
{
    postToAppThread([handle_id]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendInitialSyncReady(handle_id);
    });
}

void
matrix_notify_room_timeline_snapshot_updated(std::uint64_t handle_id, ::rust::Str room_id)
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

void
matrix_notify_sync_stopped(std::uint64_t handle_id, ::rust::Str reason, bool is_auth_error)
{
    const auto reasonStr = toQString(reason);
    postToAppThread([handle_id, reasonStr, is_auth_error]() {
        auto *mainWindow = MainWindow::instance();
        if (!mainWindow || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        auto *manager = TimelineViewManager::instance();
        if (manager)
            manager->handleMatrixBackendSyncStopped(handle_id, reasonStr, is_auth_error);
    });
}

} // namespace komai::rust_bridge
