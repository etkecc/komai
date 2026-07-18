// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendBridge.h"
#include "komai-rust-cxxbridge/ffi.h"

#include <QCoreApplication>
#include <QMetaObject>
#include <QString>
#include <QThread>

#include <utility>

#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendFfiConversions.h"
#include "matrix/backend/MatrixBackendRuntimeService.h"
#include "matrix/backend/MatrixSessionSecrets.h"
#include "profile/Paths.h"
#include "profile/ProfileSecrets.h"
#include "settings/SettingsStorage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineViewManager.h"
#include "ui/MainWindow.h"
#include "voip/CallManager.h"
#include "voip/CallTypes.h"
#include "voip/ElementCallController.h"

#ifdef ELEMENT_CALL_AVAILABLE
#include "voip/ElementCallWidgetSession.h"
#endif

namespace {

QString
toQString(::rust::Str value)
{
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

komai::MatrixNotificationItem
fromRustNotificationItem(const ::komai::rust::MatrixNotificationItem &item)
{
    return komai::MatrixNotificationItem{
      .roomId             = QString::fromStdString(std::string(item.room_id)),
      .eventId            = QString::fromStdString(std::string(item.event_id)),
      .replacementEventId = QString::fromStdString(std::string(item.replacement_event_id)),
      .roomName           = QString::fromStdString(std::string(item.room_name)),
      .avatarUrl =
        komai::matrix::normalizeMxcUri(QString::fromStdString(std::string(item.avatar_url))),
      .senderDisplayName = QString::fromStdString(std::string(item.sender_display_name)),
      .notificationKind  = QString::fromStdString(std::string(item.notification_kind)),
      .plainBody         = QString::fromStdString(std::string(item.plain_body)),
      .formattedBody     = QString::fromStdString(std::string(item.formatted_body)),
      .mediaMxcUrl =
        komai::matrix::normalizeMxcUri(QString::fromStdString(std::string(item.media_mxc_url))),
      .isReply         = item.is_reply,
      .isEmote         = item.is_emote,
      .isEncrypted     = item.is_encrypted,
      .containsSpoiler = item.contains_spoiler,
      .hasInlineImage  = item.has_inline_image,
      .playSound       = item.play_sound,
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
        if (auto logger = komai::logging::rust(); logger) {
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
settings_profile_directory(::rust::Str profile_id)
{
    return ::rust::String(settings::storage::profileDirPath(toQString(profile_id)).toStdString());
}

::rust::String
settings_secure_store_key(::rust::Str profile_id, ::rust::Str key_name)
{
    const auto keyName = std::string(key_name);
    return ::rust::String(
      settings::storage::secureStoreKey(toQString(profile_id), keyName.c_str()).toStdString());
}

::komai::rust::SettingsOptionalString
settings_read_secure_value(::rust::Str key)
{
    const auto value = settings::storage::readSecureValue(toQString(key));
    return {
      .has_value = value.has_value(),
      .value     = ::rust::String(value ? value->toStdString() : std::string()),
    };
}

void
settings_write_secure_value(::rust::Str key, ::rust::Str value)
{
    settings::storage::writeSecureValue(toQString(key), toQString(value));
}

bool
settings_write_secure_value_blocking(::rust::Str key, ::rust::Str value)
{
    return settings::storage::writeSecureValueBlocking(toQString(key), toQString(value));
}

void
settings_delete_secure_value(::rust::Str key)
{
    settings::storage::deleteSecureValue(toQString(key));
}

bool
settings_delete_secure_value_blocking(::rust::Str key)
{
    return settings::storage::deleteSecureValueBlocking(toQString(key));
}

::rust::String
settings_read_text_file(::rust::Str path, ::rust::Str label)
{
    const auto labelString = std::string(label);
    return ::rust::String(
      settings::storage::readTextFile(toQString(path), labelString.c_str()).toStdString());
}

bool
settings_path_exists(::rust::Str path)
{
    return settings::storage::pathExists(toQString(path));
}

bool
settings_remove_path(::rust::Str path)
{
    return settings::storage::removePath(toQString(path));
}

bool
settings_write_text_file(::rust::Str path, ::rust::Str content, bool owner_read_write_only)
{
    return settings::storage::writeTextFile(
      toQString(path), toQString(content), owner_read_write_only);
}

bool
settings_delete_all_profile_secrets_from_store(::rust::Str profile_id,
                                               bool uses_file_secrets_provider)
{
    return profile_secrets::deleteAllProfileSecretsFromStoreBlocking(toQString(profile_id),
                                                                     uses_file_secrets_provider);
}

::komai::rust::MatrixPersistedSessionSecrets
matrix_load_session_secrets(::rust::Str profile_id)
{
    const auto secrets = matrix_backend::loadPersistedMatrixSessionSecrets(toQString(profile_id));
    return {
      .store_passphrase   = ::rust::String(secrets.storePassphrase.toStdString()),
      .homeserver_url     = ::rust::String(secrets.homeserverUrl.toStdString()),
      .serialized_session = ::rust::String(secrets.serializedSession.toStdString()),
    };
}

bool
matrix_save_session_secrets(::rust::Str profile_id,
                            ::rust::Str store_passphrase,
                            ::rust::Str homeserver_url,
                            ::rust::Str serialized_session)
{
    // Session refresh callbacks may run while the UI thread is already inside a
    // synchronous runtime().block_on(...) call. Do not bounce persistence back
    // to the app thread here or we can deadlock the refresh path.
    return matrix_backend::savePersistedMatrixSessionSecrets(
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
matrix_notify_room_list_snapshot_updated(std::uint64_t handle_id,
                                         ::rust::Vec<::komai::rust::MatrixRoomSummary> room_list)
{
    QVector<komai::MatrixRoomSummary> snapshot;
    snapshot.reserve(static_cast<int>(room_list.size()));
    for (const auto &room : room_list)
        snapshot.push_back(komai::fromFfiRoomSummary(room));

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
matrix_notify_room_previews_backfilled(std::uint64_t handle_id,
                                       ::rust::Vec<::komai::rust::MatrixRoomPreviewUpdate> updates)
{
    struct Update
    {
        QString roomId;
        QString latestEventId;
        QString lastMessage;
        QString lastMessageKind;
        QString lastMessageSenderId;
        QString lastMessageSenderDisplayName;
        uint64_t timestamp;
    };
    QVector<Update> converted;
    converted.reserve(static_cast<int>(updates.size()));
    for (const auto &u : updates) {
        converted.push_back({
          toQString(u.room_id),
          toQString(u.latest_event_id),
          toQString(u.last_message),
          toQString(u.last_message_kind),
          toQString(u.last_message_sender_id),
          toQString(u.last_message_sender_display_name),
          u.timestamp,
        });
    }

    postToAppThread([handle_id, converted = std::move(converted)]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;
        auto *rooms = manager->rooms();
        if (!rooms)
            return;
        auto &joinedRooms = rooms->matrixJoinedRooms();
        for (const auto &u : converted) {
            auto it = joinedRooms.find(u.roomId);
            if (it == joinedRooms.end())
                continue;
            if (it->timestamp > 0)
                continue;
            it->timestamp                    = u.timestamp;
            it->lastMessage                  = u.lastMessage;
            it->lastMessageKind              = u.lastMessageKind;
            it->lastMessageSenderId          = u.lastMessageSenderId;
            it->lastMessageSenderDisplayName = u.lastMessageSenderDisplayName;
            it->latestEventId                = u.latestEventId;
        }
        rooms->notifyRoomPreviewsBackfilled();
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
matrix_notify_sync_connection_state_changed(std::uint64_t handle_id, bool is_connected)
{
    postToAppThread([handle_id, is_connected]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendSyncConnectionStateChanged(handle_id, is_connected);
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
matrix_notify_room_timeline_pagination_state(std::uint64_t handle_id,
                                             ::rust::Str room_id,
                                             bool in_progress)
{
    const auto roomId = toQString(room_id);
    postToAppThread([handle_id, roomId, in_progress]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendRoomTimelinePaginationStateChanged(
          handle_id, roomId, in_progress);
    });
}

void
matrix_notify_room_pinned_events_changed(std::uint64_t handle_id,
                                         ::rust::Str room_id,
                                         ::rust::Vec<::rust::String> event_ids)
{
    const auto roomId = toQString(room_id);
    QStringList eventIds;
    eventIds.reserve(static_cast<int>(event_ids.size()));
    for (const auto &eventId : event_ids)
        eventIds.push_back(QString::fromStdString(std::string(eventId)));

    postToAppThread([handle_id, roomId, eventIds = std::move(eventIds)]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendRoomPinnedEventsChanged(handle_id, roomId, eventIds);
    });
}

void
matrix_notify_thread_timeline_snapshot_updated(std::uint64_t handle_id,
                                               ::rust::Str room_id,
                                               ::rust::Str thread_root_id)
{
    const auto roomId       = toQString(room_id);
    const auto threadRootId = toQString(thread_root_id);
    postToAppThread([handle_id, roomId, threadRootId]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendThreadTimelineSnapshotUpdated(handle_id, roomId, threadRootId);
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

void
matrix_notify_notification_item_received(std::uint64_t handle_id,
                                         ::komai::rust::MatrixNotificationItem item)
{
    const auto notification = fromRustNotificationItem(item);
    postToAppThread([handle_id, notification]() {
        auto *mainWindow = MainWindow::instance();
        auto *chatPage   = ChatPage::instance();
        if (!mainWindow || !chatPage || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        chatPage->dispatchMatrixNotification(notification);
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

void
matrix_notify_typing_users_updated(std::uint64_t handle_id,
                                   ::rust::Str room_id,
                                   ::rust::Vec<::rust::String> display_names)
{
    const auto roomId = toQString(room_id);
    QStringList users;
    users.reserve(static_cast<int>(display_names.size()));
    for (const auto &name : display_names)
        users.push_back(QString::fromStdString(std::string(name)));

    postToAppThread([handle_id, roomId, users = std::move(users)]() {
        auto *mainWindow = MainWindow::instance();
        auto *manager    = TimelineViewManager::instance();
        if (!mainWindow || !manager || mainWindow->matrixBackendHandleId() != handle_id)
            return;

        manager->handleMatrixBackendTypingUsersUpdated(handle_id, roomId, users);
    });
}

void
matrix_notify_element_call_widget_url_ready(std::uint64_t session_id, ::rust::Str url)
{
#ifdef ELEMENT_CALL_AVAILABLE
    const auto urlStr = toQString(url);
    postToAppThread(
      [session_id, urlStr]() { ElementCallWidgetSession::deliverUrlReady(session_id, urlStr); });
#else
    (void)session_id;
    (void)url;
#endif
}

void
matrix_notify_element_call_widget_message(std::uint64_t session_id, ::rust::Str message)
{
#ifdef ELEMENT_CALL_AVAILABLE
    const auto messageStr = toQString(message);
    postToAppThread([session_id, messageStr]() {
        ElementCallWidgetSession::deliverMessage(session_id, messageStr);
    });
#else
    (void)session_id;
    (void)message;
#endif
}

void
matrix_notify_element_call_widget_stopped(std::uint64_t session_id, ::rust::Str reason)
{
#ifdef ELEMENT_CALL_AVAILABLE
    const auto reasonStr = toQString(reason);
    postToAppThread([session_id, reasonStr]() {
        ElementCallWidgetSession::deliverStopped(session_id, reasonStr);
    });
#else
    (void)session_id;
    (void)reason;
#endif
}

void
matrix_notify_rtc_notification(std::uint64_t handle_id,
                               ::komai::rust::MatrixRtcNotificationEvent event)
{
    const auto roomId  = toQString(::rust::Str(event.room_id.data(), event.room_id.size()));
    const auto eventId = toQString(::rust::Str(event.event_id.data(), event.event_id.size()));
    const auto sender  = toQString(::rust::Str(event.sender_id.data(), event.sender_id.size()));
    const auto type =
      toQString(::rust::Str(event.notification_type.data(), event.notification_type.size()));
    const auto isSelf           = event.is_self;
    const auto mentionsMe       = event.mentions_me;
    const auto expiresAtMs      = event.expires_at_ms;
    const auto notificationMode = event.notification_mode;

    postToAppThread([handle_id,
                     roomId,
                     eventId,
                     sender,
                     type,
                     isSelf,
                     mentionsMe,
                     expiresAtMs,
                     notificationMode]() {
        auto *mainWindow = MainWindow::instance();
        if (!mainWindow || mainWindow->matrixBackendHandleId() != handle_id)
            return;
        if (auto *controller = ElementCallController::instance())
            controller->onRtcNotification(
              roomId, eventId, sender, type, isSelf, mentionsMe, expiresAtMs, notificationMode);
    });
}

void
matrix_notify_rtc_decline(std::uint64_t handle_id, ::komai::rust::MatrixRtcDeclineEvent event)
{
    const auto eventId = toQString(
      ::rust::Str(event.notification_event_id.data(), event.notification_event_id.size()));
    const auto isSelf = event.is_self;

    postToAppThread([handle_id, eventId, isSelf]() {
        auto *mainWindow = MainWindow::instance();
        if (!mainWindow || mainWindow->matrixBackendHandleId() != handle_id)
            return;
        if (auto *controller = ElementCallController::instance())
            controller->onRtcDecline(eventId, isSelf);
    });
}

} // namespace komai::rust_bridge
