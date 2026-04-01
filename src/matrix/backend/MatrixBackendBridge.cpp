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

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixSessionSecrets.h"
#include "profile/Paths.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "voip/CallManager.h"
#include "voip/CallTypes.h"

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
matrix_notify_ignored_user_list_updated(std::uint64_t handle_id,
                                        ::rust::Vec<::rust::String> user_ids)
{
    QVector<QString> users;
    users.reserve(static_cast<int>(user_ids.size()));
    for (const auto &userId : user_ids)
        users.push_back(QString::fromStdString(std::string(userId)));

    postToAppThread([handle_id, users = std::move(users)]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendIgnoredUsersUpdated(handle_id, users);
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
matrix_notify_notification_received(std::uint64_t handle_id,
                                    ::rust::Str room_id,
                                    ::rust::Str event_id)
{
    const auto roomId  = toQString(room_id);
    const auto eventId = toQString(event_id);
    postToAppThread([handle_id, roomId, eventId]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendNotificationReceived(handle_id, roomId, eventId);
    });
}

namespace {
inline std::string
toStdString(const ::rust::String &value)
{
    return std::string(value.data(), value.size());
}

inline CallManager *
validCallManager(std::uint64_t handle_id)
{
    auto *mainWindow = MainWindow::instance();
    auto *chatPage   = ChatPage::instance();
    auto *manager    = chatPage ? chatPage->callManager() : nullptr;
    if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
        return nullptr;
    return manager;
}
} // namespace

void
matrix_notify_call_invite_received(std::uint64_t handle_id,
                                   ::komai::rust::MatrixCallInviteEvent event)
{
    const auto roomId    = toQString(::rust::Str(event.room_id.data(), event.room_id.size()));
    const auto senderId  = toQString(::rust::Str(event.sender_id.data(), event.sender_id.size()));
    const auto eventId   = toQString(::rust::Str(event.event_id.data(), event.event_id.size()));
    const auto callId    = toStdString(event.call_id);
    const auto partyId   = toStdString(event.party_id);
    const auto version   = toStdString(event.version);
    const auto lifetime  = event.lifetime;
    const auto invitee   = toStdString(event.invitee);
    const auto offerSdp  = toStdString(event.offer.sdp);
    const auto offerType = toStdString(event.offer.sdp_type);

    postToAppThread([handle_id,
                     roomId,
                     senderId,
                     eventId,
                     callId,
                     partyId,
                     version,
                     lifetime,
                     invitee,
                     offerSdp,
                     offerType]() {
        auto *manager = validCallManager(handle_id);
        if (!manager)
            return;

        manager->handleCallInvite(roomId,
                                  senderId,
                                  eventId,
                                  callId,
                                  partyId,
                                  version,
                                  lifetime,
                                  invitee,
                                  offerSdp,
                                  offerType);
    });
}

void
matrix_notify_call_candidates_received(std::uint64_t handle_id,
                                       ::komai::rust::MatrixCallCandidatesEvent event)
{
    const auto roomId   = toQString(::rust::Str(event.room_id.data(), event.room_id.size()));
    const auto senderId = toQString(::rust::Str(event.sender_id.data(), event.sender_id.size()));
    const auto eventId  = toQString(::rust::Str(event.event_id.data(), event.event_id.size()));
    const auto callId   = toStdString(event.call_id);
    const auto partyId  = toStdString(event.party_id);
    const auto version  = toStdString(event.version);

    komai::voip::CallIceCandidateList candidates;
    candidates.reserve(event.candidates.size());
    for (const auto &c : event.candidates) {
        candidates.push_back(komai::voip::CallIceCandidate{
          .sdpMid        = toStdString(c.sdp_mid),
          .sdpMLineIndex = c.sdp_m_line_index,
          .candidate     = toStdString(c.candidate),
        });
    }

    postToAppThread([handle_id,
                     roomId,
                     senderId,
                     eventId,
                     callId,
                     partyId,
                     version,
                     candidates = std::move(candidates)]() {
        auto *manager = validCallManager(handle_id);
        if (!manager)
            return;

        manager->handleCallCandidates(
          roomId, senderId, eventId, callId, partyId, version, candidates);
    });
}

void
matrix_notify_call_answer_received(std::uint64_t handle_id,
                                   ::komai::rust::MatrixCallAnswerEvent event)
{
    const auto roomId     = toQString(::rust::Str(event.room_id.data(), event.room_id.size()));
    const auto senderId   = toQString(::rust::Str(event.sender_id.data(), event.sender_id.size()));
    const auto eventId    = toQString(::rust::Str(event.event_id.data(), event.event_id.size()));
    const auto callId     = toStdString(event.call_id);
    const auto partyId    = toStdString(event.party_id);
    const auto version    = toStdString(event.version);
    const auto answerSdp  = toStdString(event.answer.sdp);
    const auto answerType = toStdString(event.answer.sdp_type);

    postToAppThread(
      [handle_id, roomId, senderId, eventId, callId, partyId, version, answerSdp, answerType]() {
          auto *manager = validCallManager(handle_id);
          if (!manager)
              return;

          manager->handleCallAnswer(
            roomId, senderId, eventId, callId, partyId, version, answerSdp, answerType);
      });
}

void
matrix_notify_call_hangup_received(std::uint64_t handle_id,
                                   ::komai::rust::MatrixCallHangUpEvent event)
{
    const auto roomId   = toQString(::rust::Str(event.room_id.data(), event.room_id.size()));
    const auto senderId = toQString(::rust::Str(event.sender_id.data(), event.sender_id.size()));
    const auto eventId  = toQString(::rust::Str(event.event_id.data(), event.event_id.size()));
    const auto callId   = toStdString(event.call_id);
    const auto partyId  = toStdString(event.party_id);
    const auto version  = toStdString(event.version);
    const auto reason   = toStdString(event.reason);

    postToAppThread([handle_id, roomId, senderId, eventId, callId, partyId, version, reason]() {
        auto *manager = validCallManager(handle_id);
        if (!manager)
            return;

        manager->handleCallHangUp(roomId, senderId, eventId, callId, partyId, version, reason);
    });
}

void
matrix_notify_call_select_answer_received(std::uint64_t handle_id,
                                          ::komai::rust::MatrixCallSelectAnswerEvent event)
{
    const auto roomId   = toQString(::rust::Str(event.room_id.data(), event.room_id.size()));
    const auto senderId = toQString(::rust::Str(event.sender_id.data(), event.sender_id.size()));
    const auto eventId  = toQString(::rust::Str(event.event_id.data(), event.event_id.size()));
    const auto callId   = toStdString(event.call_id);
    const auto partyId  = toStdString(event.party_id);
    const auto version  = toStdString(event.version);
    const auto selectedPartyId = toStdString(event.selected_party_id);

    postToAppThread(
      [handle_id, roomId, senderId, eventId, callId, partyId, version, selectedPartyId]() {
          auto *manager = validCallManager(handle_id);
          if (!manager)
              return;

          manager->handleCallSelectAnswer(
            roomId, senderId, eventId, callId, partyId, version, selectedPartyId);
      });
}

void
matrix_notify_call_reject_received(std::uint64_t handle_id,
                                   ::komai::rust::MatrixCallRejectEvent event)
{
    const auto roomId   = toQString(::rust::Str(event.room_id.data(), event.room_id.size()));
    const auto senderId = toQString(::rust::Str(event.sender_id.data(), event.sender_id.size()));
    const auto eventId  = toQString(::rust::Str(event.event_id.data(), event.event_id.size()));
    const auto callId   = toStdString(event.call_id);
    const auto partyId  = toStdString(event.party_id);
    const auto version  = toStdString(event.version);

    postToAppThread([handle_id, roomId, senderId, eventId, callId, partyId, version]() {
        auto *manager = validCallManager(handle_id);
        if (!manager)
            return;

        manager->handleCallReject(roomId, senderId, eventId, callId, partyId, version);
    });
}

void
matrix_notify_call_negotiate_received(std::uint64_t handle_id,
                                      ::komai::rust::MatrixCallNegotiateEvent event)
{
    const auto roomId   = toQString(::rust::Str(event.room_id.data(), event.room_id.size()));
    const auto senderId = toQString(::rust::Str(event.sender_id.data(), event.sender_id.size()));
    const auto eventId  = toQString(::rust::Str(event.event_id.data(), event.event_id.size()));
    const auto callId   = toStdString(event.call_id);
    const auto partyId  = toStdString(event.party_id);
    const auto lifetime = event.lifetime;
    const auto descSdp  = toStdString(event.description.sdp);
    const auto descType = toStdString(event.description.sdp_type);

    postToAppThread(
      [handle_id, roomId, senderId, eventId, callId, partyId, lifetime, descSdp, descType]() {
          auto *manager = validCallManager(handle_id);
          if (!manager)
              return;

          manager->handleCallNegotiate(
            roomId, senderId, eventId, callId, partyId, lifetime, descSdp, descType);
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
