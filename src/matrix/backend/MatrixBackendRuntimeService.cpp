// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include "komai-rust-cxxbridge/lib.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendBridge.h"

namespace komai {

namespace {
::rust::Vec<::rust::String>
toRustStringVec(const QVector<QString> &values)
{
    ::rust::Vec<::rust::String> rustValues;
    for (const auto &value : values)
        rustValues.push_back(value.toStdString());
    return rustValues;
}

MatrixBackendHandleInfo
fromRustHandleInfo(const ::komai::rust::MatrixBackendHandleInfo &info)
{
    return MatrixBackendHandleInfo{
      .handleId      = info.handle_id,
      .hasSession    = info.has_session,
      .homeserverUrl = QString::fromStdString(std::string(info.homeserver_url)),
      .userId        = QString::fromStdString(std::string(info.user_id)),
      .deviceId      = QString::fromStdString(std::string(info.device_id)),
    };
}

MatrixOwnProfile
fromRustOwnProfile(const ::komai::rust::MatrixOwnProfile &profile)
{
    return MatrixOwnProfile{
      .displayName = QString::fromStdString(std::string(profile.display_name)),
      .avatarUrl = matrix::normalizeMxcUri(QString::fromStdString(std::string(profile.avatar_url))),
    };
}

MatrixRoomSummary
fromRustRoomSummary(const ::komai::rust::MatrixRoomSummary &room)
{
    return MatrixRoomSummary{
      .roomId      = QString::fromStdString(std::string(room.room_id)),
      .displayName = QString::fromStdString(std::string(room.display_name)),
      .avatarUrl   = matrix::normalizeMxcUri(QString::fromStdString(std::string(room.avatar_url))),
      .topic       = QString::fromStdString(std::string(room.topic)),
      .directChatOtherUserId = QString::fromStdString(std::string(room.direct_chat_other_user_id)),
      .isInvite              = room.is_invite,
      .isSpace               = room.is_space,
      .isDirect              = room.is_direct,
      .isBotRoom             = room.is_bot_room,
      .isEncrypted           = room.is_encrypted,
      .unreadMessages        = room.unread_message_count,
      .notificationCount     = room.notification_count,
      .highlightCount        = room.highlight_count,
      .timestamp             = room.timestamp,
    };
}

MatrixTimelineItem
fromRustTimelineItem(const ::komai::rust::MatrixTimelineItem &item)
{
    return MatrixTimelineItem{
      .itemId            = QString::fromStdString(std::string(item.item_id)),
      .eventId           = QString::fromStdString(std::string(item.event_id)),
      .senderId          = QString::fromStdString(std::string(item.sender_id)),
      .senderDisplayName = QString::fromStdString(std::string(item.sender_display_name)),
      .senderAvatarUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.sender_avatar_url))),
      .body      = QString::fromStdString(std::string(item.body)),
      .itemKind  = QString::fromStdString(std::string(item.item_kind)),
      .timestamp = item.timestamp,
      .isOwn     = item.is_own,
    };
}

MatrixJoinRoomResult
fromRustJoinRoomResult(const ::komai::rust::MatrixJoinRoomResult &result)
{
    return MatrixJoinRoomResult{
      .ok            = result.ok,
      .roomId        = QString::fromStdString(std::string(result.room_id)),
      .error         = QString::fromStdString(std::string(result.error)),
      .matrixErrcode = QString::fromStdString(std::string(result.matrix_errcode)),
    };
}

::rust::String
toRustCreateRoomPreset(MatrixCreateRoomPreset preset)
{
    switch (preset) {
    case MatrixCreateRoomPreset::PublicChat:
        return ::rust::String("public_chat");
    case MatrixCreateRoomPreset::TrustedPrivateChat:
        return ::rust::String("trusted_private_chat");
    case MatrixCreateRoomPreset::PrivateChat:
    default:
        return ::rust::String("private_chat");
    }
}

} // namespace

std::optional<MatrixBackendHandleInfo>
MatrixBackendRuntimeService::startRestoredBackend(const QString &profileId, QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_start_restored_backend(profileId.toStdString());
        return fromRustHandleInfo(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::stopBackend(uint64_t handleId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_stop_backend(handleId);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
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

MatrixJoinRoomResult
MatrixBackendRuntimeService::joinRoom(uint64_t handleId,
                                      const QString &roomIdOrAlias,
                                      const QVector<QString> &via,
                                      const QString &reason)
{
    try {
        const auto result = ::komai::rust::matrix_join_room(
          handleId, roomIdOrAlias.toStdString(), toRustStringVec(via), reason.toStdString());
        return fromRustJoinRoomResult(result);
    } catch (const std::exception &e) {
        return MatrixJoinRoomResult{
          .ok            = false,
          .roomId        = roomIdOrAlias,
          .error         = QString::fromUtf8(e.what()),
          .matrixErrcode = {},
        };
    }
}

std::optional<QString>
MatrixBackendRuntimeService::knockRoom(uint64_t handleId,
                                       const QString &roomIdOrAlias,
                                       const QVector<QString> &via,
                                       const QString &reason,
                                       QString *errorOut)
{
    try {
        const auto result = ::komai::rust::matrix_knock_room(
          handleId, roomIdOrAlias.toStdString(), toRustStringVec(via), reason.toStdString());
        return QString::fromStdString(std::string(result));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QString>
MatrixBackendRuntimeService::createRoom(uint64_t handleId,
                                        const MatrixCreateRoomRequest &request,
                                        QString *errorOut)
{
    try {
        const auto result =
          ::komai::rust::matrix_create_room(handleId,
                                            request.name.toStdString(),
                                            request.topic.toStdString(),
                                            request.roomAliasLocalpart.toStdString(),
                                            toRustStringVec(request.inviteUserIds),
                                            toRustCreateRoomPreset(request.preset),
                                            request.isDirect,
                                            request.isEncrypted,
                                            request.isSpace,
                                            request.isPublic);
        return QString::fromStdString(std::string(result));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::leaveRoom(uint64_t handleId,
                                       const QString &roomId,
                                       const QString &reason,
                                       QString *errorOut)
{
    try {
        ::komai::rust::matrix_leave_room(handleId, roomId.toStdString(), reason.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::inviteUser(uint64_t handleId,
                                        const QString &roomId,
                                        const QString &userId,
                                        const QString &reason,
                                        QString *errorOut)
{
    try {
        ::komai::rust::matrix_invite_user(
          handleId, roomId.toStdString(), userId.toStdString(), reason.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::kickUser(uint64_t handleId,
                                      const QString &roomId,
                                      const QString &userId,
                                      const QString &reason,
                                      QString *errorOut)
{
    try {
        ::komai::rust::matrix_kick_user(
          handleId, roomId.toStdString(), userId.toStdString(), reason.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::banUser(uint64_t handleId,
                                     const QString &roomId,
                                     const QString &userId,
                                     const QString &reason,
                                     QString *errorOut)
{
    try {
        ::komai::rust::matrix_ban_user(
          handleId, roomId.toStdString(), userId.toStdString(), reason.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unbanUser(uint64_t handleId,
                                       const QString &roomId,
                                       const QString &userId,
                                       const QString &reason,
                                       QString *errorOut)
{
    try {
        ::komai::rust::matrix_unban_user(
          handleId, roomId.toStdString(), userId.toStdString(), reason.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixOwnProfile>
MatrixBackendRuntimeService::fetchOwnProfile(uint64_t handleId, QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_fetch_own_profile(handleId);
        return fromRustOwnProfile(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<MatrixRoomSummary>>
MatrixBackendRuntimeService::fetchRoomList(uint64_t handleId, QString *errorOut)
{
    try {
        const auto result = ::komai::rust::matrix_fetch_room_list(handleId);
        QVector<MatrixRoomSummary> rooms;
        rooms.reserve(static_cast<int>(result.size()));
        for (const auto &room : result)
            rooms.push_back(fromRustRoomSummary(room));
        return rooms;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::selectActiveRoomTimeline(uint64_t handleId,
                                                      const QString &roomId,
                                                      QString *errorOut)
{
    try {
        ::komai::rust::matrix_select_active_room_timeline(handleId, roomId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QVector<MatrixTimelineItem>>
MatrixBackendRuntimeService::fetchActiveRoomTimeline(uint64_t handleId, QString *errorOut)
{
    try {
        const auto result = ::komai::rust::matrix_fetch_active_room_timeline(handleId);
        QVector<MatrixTimelineItem> items;
        items.reserve(static_cast<int>(result.size()));
        for (const auto &item : result)
            items.push_back(fromRustTimelineItem(item));
        return items;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::paginateActiveRoomTimelineBackwards(uint64_t handleId,
                                                                 uint16_t pageSize,
                                                                 QString *errorOut)
{
    try {
        ::komai::rust::matrix_paginate_active_room_timeline_backwards(handleId, pageSize);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::sendRoomMessage(uint64_t handleId,
                                             const QString &roomId,
                                             const QString &body,
                                             const QString &formattedHtml,
                                             const QString &messageKind,
                                             QString *errorOut)
{
    try {
        ::komai::rust::matrix_send_room_message(handleId,
                                                roomId.toStdString(),
                                                body.toStdString(),
                                                formattedHtml.toStdString(),
                                                messageKind.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QByteArray>
MatrixBackendRuntimeService::fetchMediaContent(uint64_t handleId,
                                               const QString &mxcUri,
                                               int width,
                                               int height,
                                               bool crop,
                                               QString *errorOut)
{
    try {
        const auto result = ::komai::rust::matrix_fetch_media_content(
          handleId, mxcUri.toStdString(), width, height, crop);
        QByteArray data;
        data.reserve(static_cast<qsizetype>(result.size()));
        data.append(reinterpret_cast<const char *>(result.data()),
                    static_cast<qsizetype>(result.size()));
        return data;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

} // namespace komai
