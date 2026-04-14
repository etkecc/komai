// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendBridge.h"
#include "matrix/backend/MatrixBackendRuntimeServiceInternal.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "profile/ProfileId.h"

namespace komai {

namespace {
QHash<uint64_t, QVector<MatrixRoomSummary>> cachedRoomListSnapshots;

QString
normalizeProfileId(QStringView profileId)
{
    return profile_id::normalized(profileId);
}

MatrixBackendHandleInfo
fromRustHandleInfo(const ::komai::rust::MatrixBackendHandleInfo &info)
{
    return MatrixBackendHandleInfo{
      .handleId      = info.handle_id,
      .hasSession    = info.has_session,
      .authType      = QString::fromStdString(std::string(info.auth_type)),
      .homeserverUrl = QString::fromStdString(std::string(info.homeserver_url)),
      .userId        = QString::fromStdString(std::string(info.user_id)),
      .deviceId      = QString::fromStdString(std::string(info.device_id)),
    };
}

} // anonymous namespace

MatrixRoomSummary
fromFfiRoomSummary(const ::komai::rust::MatrixRoomSummary &room)
{
    QVector<QString> tags;
    tags.reserve(static_cast<int>(room.tags.size()));
    for (const auto &value : room.tags)
        tags.push_back(QString::fromStdString(std::string(value)));

    QVector<QString> parentSpaceRoomIds;
    parentSpaceRoomIds.reserve(static_cast<int>(room.parent_space_room_ids.size()));
    for (const auto &value : room.parent_space_room_ids)
        parentSpaceRoomIds.push_back(QString::fromStdString(std::string(value)));

    return MatrixRoomSummary{
      .roomId        = QString::fromStdString(std::string(room.room_id)),
      .latestEventId = QString::fromStdString(std::string(room.latest_event_id)),
      .displayName   = QString::fromStdString(std::string(room.display_name)),
      .avatarUrl   = matrix::normalizeMxcUri(QString::fromStdString(std::string(room.avatar_url))),
      .topic       = QString::fromStdString(std::string(room.topic)),
      .roomAlias   = QString::fromStdString(std::string(room.room_alias)),
      .lastMessage = QString::fromStdString(std::string(room.last_message)),
      .lastMessageKind     = QString::fromStdString(std::string(room.last_message_kind)),
      .lastMessageSenderId = QString::fromStdString(std::string(room.last_message_sender_id)),
      .lastMessageSenderDisplayName =
        QString::fromStdString(std::string(room.last_message_sender_display_name)),
      .tags                  = std::move(tags),
      .parentSpaceRoomIds    = std::move(parentSpaceRoomIds),
      .directChatOtherUserId = QString::fromStdString(std::string(room.direct_chat_other_user_id)),
      .inviterUserId         = QString::fromStdString(std::string(room.inviter_user_id)),
      .inviterDisplayName    = QString::fromStdString(std::string(room.inviter_display_name)),
      .inviterAvatarUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(room.inviter_avatar_url))),
      .inviteReason      = QString::fromStdString(std::string(room.invite_reason)),
      .isInvite          = room.is_invite,
      .isSpace           = room.is_space,
      .isDirect          = room.is_direct,
      .isBotRoom         = room.is_bot_room,
      .isEncrypted       = room.is_encrypted,
      .isPublic          = room.is_public,
      .memberCount       = room.member_count,
      .unreadMessages    = room.unread_message_count,
      .notificationCount = room.notification_count,
      .highlightCount    = room.highlight_count,
      .timestamp         = room.timestamp,
    };
}

std::optional<MatrixBackendHandleInfo>
MatrixBackendRuntimeService::startRestoredBackend(matrix_backend::BlockingCallContext context,
                                                  const QString &profileId,
                                                  QString *errorOut)
{
    try {
        const auto normalizedProfileId = normalizeProfileId(profileId);
        auto result                    = invokeRuntimeWorkerCall(
          "matrix_start_restored_backend", [context, &normalizedProfileId]() {
              return ::komai::rust::matrix_start_restored_backend(
                matrix_backend::toRustBlockingContext(context), normalizedProfileId.toStdString());
          });
        return fromRustHandleInfo(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::logoutBackend(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_logout_backend", [context, handleId]() {
            ::komai::rust::matrix_logout_backend(matrix_backend::toRustBlockingContext(context),
                                                 handleId);
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::stopBackend(uint64_t handleId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_stop_backend(handleId);
        clearCachedRoomListSnapshot(handleId);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<uint16_t>
MatrixBackendRuntimeService::startMediaProxy(uint64_t handleId)
{
    try {
        return ::komai::rust::matrix_start_media_proxy(handleId);
    } catch (const std::exception &e) {
        nhlog::net()->warn("Failed to start media proxy for handle {}: {}", handleId, e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::isTimelineMediaEncrypted(uint64_t handleId, const QString &itemId)
{
    return ::komai::rust::matrix_is_timeline_media_encrypted(handleId, itemId.toStdString());
}

std::optional<QString>
MatrixBackendRuntimeService::registerTimelineMediaProxyUrl(uint64_t handleId,
                                                           const QString &itemId,
                                                           const QString &extension)
{
    try {
        auto url = ::komai::rust::matrix_register_timeline_media_proxy_url(
          handleId, itemId.toStdString(), extension.toStdString());
        return QString::fromStdString(std::string(url));
    } catch (const std::exception &) {
        return std::nullopt;
    }
}

void
MatrixBackendRuntimeService::stopMediaProxy(uint64_t handleId)
{
    ::komai::rust::matrix_stop_media_proxy(handleId);
}

bool
MatrixBackendRuntimeService::startSync(uint64_t handleId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_start_backend_sync(handleId);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QVector<MatrixRoomSummary>>
MatrixBackendRuntimeService::fetchRoomList(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           QString *errorOut)
{
    if (cachedRoomListSnapshots.contains(handleId))
        return cachedRoomListSnapshots.value(handleId);

    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_room_list", [context, handleId]() {
              return ::komai::rust::matrix_fetch_room_list(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        QVector<MatrixRoomSummary> rooms;
        rooms.reserve(static_cast<int>(result.size()));
        for (const auto &room : result)
            rooms.push_back(fromFfiRoomSummary(room));
        cachedRoomListSnapshots.insert(handleId, rooms);
        return rooms;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

void
MatrixBackendRuntimeService::cacheRoomListSnapshot(uint64_t handleId,
                                                   QVector<MatrixRoomSummary> rooms)
{
    cachedRoomListSnapshots.insert(handleId, std::move(rooms));
}

void
MatrixBackendRuntimeService::clearCachedRoomListSnapshot(uint64_t handleId)
{
    cachedRoomListSnapshots.remove(handleId);
}

} // namespace komai
