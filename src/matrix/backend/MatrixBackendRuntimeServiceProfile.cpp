// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include "komai-rust-cxxbridge/ffi.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendBridge.h"
#include "matrix/backend/MatrixBackendRuntimeServiceInternal.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"

namespace komai {

namespace {

MatrixOwnProfile
fromRustOwnProfile(const ::komai::rust::MatrixOwnProfile &profile)
{
    return MatrixOwnProfile{
      .displayName = QString::fromStdString(std::string(profile.display_name)),
      .avatarUrl = matrix::normalizeMxcUri(QString::fromStdString(std::string(profile.avatar_url))),
    };
}

MatrixOwnPresence
fromRustOwnPresence(const ::komai::rust::MatrixOwnPresence &presence)
{
    return MatrixOwnPresence{
      .state         = QString::fromStdString(std::string(presence.state)),
      .statusMessage = QString::fromStdString(std::string(presence.status_message)),
    };
}

MatrixTurnServerInfo
fromRustTurnServerInfo(const ::komai::rust::MatrixTurnServerInfo &info)
{
    QVector<QString> uris;
    uris.reserve(static_cast<int>(info.uris.size()));
    for (const auto &uri : info.uris)
        uris.push_back(QString::fromStdString(std::string(uri)));

    return MatrixTurnServerInfo{
      .username   = QString::fromStdString(std::string(info.username)),
      .password   = QString::fromStdString(std::string(info.password)),
      .uris       = std::move(uris),
      .ttlSeconds = info.ttl_seconds,
    };
}

MatrixUserProfile
fromRustUserProfile(const ::komai::rust::MatrixUserProfile &profile)
{
    return MatrixUserProfile{
      .displayName = QString::fromStdString(std::string(profile.display_name)),
      .avatarUrl = matrix::normalizeMxcUri(QString::fromStdString(std::string(profile.avatar_url))),
    };
}

MatrixDirectoryUser
fromRustDirectoryUser(const ::komai::rust::MatrixDirectoryUser &user)
{
    return MatrixDirectoryUser{
      .displayName = QString::fromStdString(std::string(user.display_name)),
      .userId      = QString::fromStdString(std::string(user.user_id)),
      .avatarUrl   = matrix::normalizeMxcUri(QString::fromStdString(std::string(user.avatar_url))),
    };
}

MatrixPublicRoomDirectoryEntry
fromRustPublicRoomDirectoryEntry(const ::komai::rust::MatrixPublicRoomDirectoryEntry &room)
{
    return MatrixPublicRoomDirectoryEntry{
      .roomId         = QString::fromStdString(std::string(room.room_id)),
      .roomServerName = QString::fromStdString(std::string(room.room_server_name)),
      .displayName    = QString::fromStdString(std::string(room.display_name)),
      .avatarUrl = matrix::normalizeMxcUri(QString::fromStdString(std::string(room.avatar_url))),
      .topic     = QString::fromStdString(std::string(room.topic)),
      .canonicalAlias  = QString::fromStdString(std::string(room.canonical_alias)),
      .memberCount     = room.member_count,
      .isWorldReadable = room.is_world_readable,
      .isSpace         = room.is_space,
    };
}

MatrixNotificationItem
fromRustNotificationItem(const ::komai::rust::MatrixNotificationItem &item)
{
    return MatrixNotificationItem{
      .roomId             = QString::fromStdString(std::string(item.room_id)),
      .eventId            = QString::fromStdString(std::string(item.event_id)),
      .replacementEventId = QString::fromStdString(std::string(item.replacement_event_id)),
      .roomName           = QString::fromStdString(std::string(item.room_name)),
      .avatarUrl = matrix::normalizeMxcUri(QString::fromStdString(std::string(item.avatar_url))),
      .senderDisplayName = QString::fromStdString(std::string(item.sender_display_name)),
      .notificationKind  = QString::fromStdString(std::string(item.notification_kind)),
      .plainBody         = QString::fromStdString(std::string(item.plain_body)),
      .formattedBody     = QString::fromStdString(std::string(item.formatted_body)),
      .mediaMxcUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.media_mxc_url))),
      .isReply         = item.is_reply,
      .isEmote         = item.is_emote,
      .isEncrypted     = item.is_encrypted,
      .containsSpoiler = item.contains_spoiler,
      .hasInlineImage  = item.has_inline_image,
      .playSound       = item.play_sound,
    };
}

MatrixImagePack
fromRustImagePack(const ::komai::rust::MatrixImagePack &pack)
{
    QVector<MatrixImagePackImage> images;
    images.reserve(static_cast<int>(pack.images.size()));
    for (const auto &image : pack.images) {
        images.push_back(MatrixImagePackImage{
          .shortcode = QString::fromStdString(std::string(image.shortcode)),
          .body      = QString::fromStdString(std::string(image.body)),
          .url       = matrix::normalizeMxcUri(QString::fromStdString(std::string(image.url))),
          .isEmote   = image.is_emote,
          .isSticker = image.is_sticker,
        });
    }

    return MatrixImagePack{
      .sourceRoomId = QString::fromStdString(std::string(pack.source_room_id)),
      .stateKey     = QString::fromStdString(std::string(pack.state_key)),
      .displayName  = QString::fromStdString(std::string(pack.display_name)),
      .avatarUrl    = matrix::normalizeMxcUri(QString::fromStdString(std::string(pack.avatar_url))),
      .attribution  = QString::fromStdString(std::string(pack.attribution)),
      .isEmotePack  = pack.is_emote_pack,
      .isStickerPack     = pack.is_sticker_pack,
      .fromSpace         = pack.from_space,
      .isGloballyEnabled = pack.is_globally_enabled,
      .images            = std::move(images),
    };
}

::rust::Vec<::komai::rust::MatrixNotificationRequest>
toRustNotificationRequests(const QVector<MatrixNotificationRequest> &requests)
{
    ::rust::Vec<::komai::rust::MatrixNotificationRequest> rustRequests;
    for (const auto &request : requests) {
        rustRequests.push_back(::komai::rust::MatrixNotificationRequest{
          .room_id  = request.roomId.toStdString(),
          .event_id = request.eventId.toStdString(),
        });
    }
    return rustRequests;
}

::komai::rust::MatrixImagePack
toRustImagePack(const MatrixImagePack &pack)
{
    ::rust::Vec<::komai::rust::MatrixImagePackImage> images;
    for (const auto &image : pack.images) {
        images.push_back(::komai::rust::MatrixImagePackImage{
          .shortcode  = image.shortcode.toStdString(),
          .body       = image.body.toStdString(),
          .url        = image.url.toStdString(),
          .is_emote   = image.isEmote,
          .is_sticker = image.isSticker,
        });
    }

    return ::komai::rust::MatrixImagePack{
      .source_room_id      = pack.sourceRoomId.toStdString(),
      .state_key           = pack.stateKey.toStdString(),
      .display_name        = pack.displayName.toStdString(),
      .avatar_url          = pack.avatarUrl.toStdString(),
      .attribution         = pack.attribution.toStdString(),
      .is_emote_pack       = pack.isEmotePack,
      .is_sticker_pack     = pack.isStickerPack,
      .from_space          = pack.fromSpace,
      .is_globally_enabled = pack.isGloballyEnabled,
      .images              = std::move(images),
    };
}

} // anonymous namespace

std::optional<MatrixOwnProfile>
MatrixBackendRuntimeService::fetchOwnProfile(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall("matrix_fetch_own_profile", [context, handleId]() {
            return ::komai::rust::matrix_fetch_own_profile(
              matrix_backend::toRustBlockingContext(context), handleId);
        });
        return fromRustOwnProfile(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixOwnPresence>
MatrixBackendRuntimeService::fetchOwnPresence(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall("matrix_fetch_own_presence", [context, handleId]() {
            return ::komai::rust::matrix_fetch_own_presence(
              matrix_backend::toRustBlockingContext(context), handleId);
        });
        return fromRustOwnPresence(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::setOwnDisplayName(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &displayName,
                                               QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_set_own_display_name", [context, handleId, displayName]() {
            ::komai::rust::matrix_set_own_display_name(
              matrix_backend::toRustBlockingContext(context), handleId, displayName.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setOwnPresence(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &presenceState,
                                            const QString &statusMessage,
                                            QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_set_own_presence", [context, handleId, presenceState, statusMessage]() {
              ::komai::rust::matrix_set_own_presence(matrix_backend::toRustBlockingContext(context),
                                                     handleId,
                                                     presenceState.toStdString(),
                                                     statusMessage.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setOwnRoomDisplayName(matrix_backend::BlockingCallContext context,
                                                   uint64_t handleId,
                                                   const QString &roomId,
                                                   const QString &displayName,
                                                   QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_set_own_room_display_name",
                                [context, handleId, roomId, displayName]() {
                                    ::komai::rust::matrix_set_own_room_display_name(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      displayName.toStdString());
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::uploadOwnAvatar(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &filePath,
                                             const QString &mimeType,
                                             QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_upload_own_avatar",
                                [context, handleId, filePath, mimeType]() {
                                    ::komai::rust::matrix_upload_own_avatar(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      filePath.toStdString(),
                                      mimeType.toStdString());
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::removeOwnAvatar(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_remove_own_avatar", [context, handleId]() {
            ::komai::rust::matrix_remove_own_avatar(matrix_backend::toRustBlockingContext(context),
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
MatrixBackendRuntimeService::uploadOwnRoomAvatar(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 const QString &roomId,
                                                 const QString &filePath,
                                                 const QString &mimeType,
                                                 QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_upload_own_room_avatar",
                                [context, handleId, roomId, filePath, mimeType]() {
                                    ::komai::rust::matrix_upload_own_room_avatar(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      filePath.toStdString(),
                                      mimeType.toStdString());
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::removeOwnRoomAvatar(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 const QString &roomId,
                                                 QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_remove_own_room_avatar", [context, handleId, roomId]() {
            ::komai::rust::matrix_remove_own_room_avatar(
              matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::ignoreUser(matrix_backend::BlockingCallContext context,
                                        uint64_t handleId,
                                        const QString &userId,
                                        QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_ignore_user", [context, handleId, userId]() {
            ::komai::rust::matrix_ignore_user(
              matrix_backend::toRustBlockingContext(context), handleId, userId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unignoreUser(matrix_backend::BlockingCallContext context,
                                          uint64_t handleId,
                                          const QString &userId,
                                          QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_unignore_user", [context, handleId, userId]() {
            ::komai::rust::matrix_unignore_user(
              matrix_backend::toRustBlockingContext(context), handleId, userId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixUserProfile>
MatrixBackendRuntimeService::fetchUserProfile(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &userId,
                                              QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_fetch_user_profile", [context, handleId, userId]() {
              return ::komai::rust::matrix_fetch_user_profile(
                matrix_backend::toRustBlockingContext(context), handleId, userId.toStdString());
          });
        return fromRustUserProfile(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixUserProfile>
MatrixBackendRuntimeService::fetchRoomMemberProfile(matrix_backend::BlockingCallContext context,
                                                    uint64_t handleId,
                                                    const QString &roomId,
                                                    const QString &userId,
                                                    QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_room_member_profile", [context, handleId, roomId, userId]() {
              return ::komai::rust::matrix_fetch_room_member_profile(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                userId.toStdString());
          });
        return fromRustUserProfile(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<MatrixDirectoryUser>>
MatrixBackendRuntimeService::searchUsers(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &searchTerm,
                                         uint64_t limit,
                                         QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_search_users", [context, handleId, searchTerm, limit]() {
              return ::komai::rust::matrix_search_users(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                searchTerm.toStdString(),
                limit);
          });
        QVector<MatrixDirectoryUser> users;
        users.reserve(static_cast<int>(result.size()));
        for (const auto &user : result)
            users.push_back(fromRustDirectoryUser(user));
        return users;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixPublicRoomDirectoryPage>
MatrixBackendRuntimeService::fetchPublicRoomDirectoryPage(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &searchTerm,
  uint64_t limit,
  const QString &since,
  const QString &server,
  const QString &roomTypeFilter,
  QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_public_room_directory_page",
          [context, handleId, searchTerm, limit, since, server, roomTypeFilter]() {
              return ::komai::rust::matrix_fetch_public_room_directory_page(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                searchTerm.toStdString(),
                limit,
                since.toStdString(),
                server.toStdString(),
                roomTypeFilter.toStdString());
          });
        MatrixPublicRoomDirectoryPage page;
        page.nextBatch              = QString::fromStdString(std::string(result.next_batch));
        page.totalRoomCountEstimate = result.total_room_count_estimate;
        page.rooms.reserve(static_cast<int>(result.rooms.size()));
        for (const auto &room : result.rooms)
            page.rooms.push_back(fromRustPublicRoomDirectoryEntry(room));
        return page;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<MatrixNotificationItem>>
MatrixBackendRuntimeService::fetchNotificationItems(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QVector<MatrixNotificationRequest> &requests,
  QString *errorOut)
{
    if (requests.isEmpty())
        return QVector<MatrixNotificationItem>{};

    try {
        auto rustRequests = toRustNotificationRequests(requests);
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_notification_items",
          [context, handleId, rustRequests = std::move(rustRequests)]() {
              return ::komai::rust::matrix_fetch_notification_items(
                matrix_backend::toRustBlockingContext(context), handleId, rustRequests);
          });
        QVector<MatrixNotificationItem> items;
        items.reserve(static_cast<int>(result.size()));
        for (const auto &item : result)
            items.push_back(fromRustNotificationItem(item));
        return items;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<bool>
MatrixBackendRuntimeService::fetchAccountNotificationsEnabled(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        return invokeRuntimeWorkerCall(
          "matrix_fetch_account_notifications_enabled", [context, handleId]() {
              return ::komai::rust::matrix_fetch_account_notifications_enabled(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixTurnServerInfo>
MatrixBackendRuntimeService::fetchTurnServerInfo(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_turn_server_info", [context, handleId]() {
              return ::komai::rust::matrix_fetch_turn_server_info(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return fromRustTurnServerInfo(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::setAccountNotificationsEnabled(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  bool enabled,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_set_account_notifications_enabled", [context, handleId, enabled]() {
              ::komai::rust::matrix_set_account_notifications_enabled(
                matrix_backend::toRustBlockingContext(context), handleId, enabled);
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QVector<MatrixImagePack>>
MatrixBackendRuntimeService::fetchImagePacks(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_image_packs", [context, handleId, roomId]() {
              return ::komai::rust::matrix_fetch_image_packs(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });
        QVector<MatrixImagePack> packs;
        packs.reserve(static_cast<int>(result.size()));
        for (const auto &pack : result)
            packs.push_back(fromRustImagePack(pack));
        return packs;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::saveImagePack(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           const QString &roomId,
                                           const QString &stateKey,
                                           const QString &previousStateKey,
                                           bool hasPreviousStateKey,
                                           const MatrixImagePack &pack,
                                           QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_save_image_pack",
                                [context,
                                 handleId,
                                 roomId,
                                 stateKey,
                                 previousStateKey,
                                 hasPreviousStateKey,
                                 pack = toRustImagePack(pack)]() mutable {
                                    ::komai::rust::matrix_save_image_pack(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      stateKey.toStdString(),
                                      previousStateKey.toStdString(),
                                      hasPreviousStateKey,
                                      std::move(pack));
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::removeImagePack(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &stateKey,
                                             QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_remove_image_pack",
                                [context, handleId, roomId, stateKey]() {
                                    ::komai::rust::matrix_remove_image_pack(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      stateKey.toStdString());
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setImagePackGloballyEnabled(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &roomId,
  const QString &stateKey,
  bool enabled,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_set_image_pack_globally_enabled",
                                [context, handleId, roomId, stateKey, enabled]() {
                                    ::komai::rust::matrix_set_image_pack_globally_enabled(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      stateKey.toStdString(),
                                      enabled);
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

} // namespace komai
