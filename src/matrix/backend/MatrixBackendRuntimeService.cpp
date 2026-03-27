// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include "komai-rust-cxxbridge/lib.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendBridge.h"
#include "profile/ProfileId.h"

namespace komai {

namespace {
QString
normalizeProfileId(QStringView profileId)
{
    return profile_id::normalized(profileId);
}

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
      .authType      = QString::fromStdString(std::string(info.auth_type)),
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

MatrixRecoveryStatus
fromRustRecoveryStatus(const ::komai::rust::MatrixRecoveryStatus &status)
{
    return MatrixRecoveryStatus{
      .state                     = QString::fromStdString(std::string(status.state)),
      .hasDevicesToVerifyAgainst = status.has_devices_to_verify_against,
      .ownDeviceIsVerified       = status.own_device_is_verified,
      .hasUnverifiedOwnDevices   = status.has_unverified_own_devices,
    };
}

MatrixSetupRecoveryResult
fromRustSetupRecoveryResult(const ::komai::rust::MatrixSetupRecoveryResult &result)
{
    return MatrixSetupRecoveryResult{
      .recoveryKey = QString::fromStdString(std::string(result.recovery_key)),
    };
}

MatrixResetEncryptionIdentityResult
fromRustResetEncryptionIdentityResult(
  const ::komai::rust::MatrixResetEncryptionIdentityResult &result)
{
    return MatrixResetEncryptionIdentityResult{
      .completed   = result.completed,
      .authType    = QString::fromStdString(std::string(result.auth_type)),
      .approvalUrl = QString::fromStdString(std::string(result.approval_url)),
    };
}

MatrixDeviceSignOutResult
fromRustDeviceSignOutResult(const ::komai::rust::MatrixDeviceSignOutResult &result)
{
    return MatrixDeviceSignOutResult{
      .completed   = result.completed,
      .authType    = QString::fromStdString(std::string(result.auth_type)),
      .approvalUrl = QString::fromStdString(std::string(result.approval_url)),
    };
}

MatrixVerificationSession
fromRustVerificationSession(const ::komai::rust::MatrixVerificationSession &session)
{
    QVector<int> sasNumbers;
    sasNumbers.reserve(static_cast<int>(session.sas_numbers.size()));
    for (const auto number : session.sas_numbers)
        sasNumbers.push_back(static_cast<int>(number));

    return MatrixVerificationSession{
      .flowId                    = QString::fromStdString(std::string(session.flow_id)),
      .userId                    = QString::fromStdString(std::string(session.user_id)),
      .deviceId                  = QString::fromStdString(std::string(session.device_id)),
      .state                     = QString::fromStdString(std::string(session.state)),
      .error                     = QString::fromStdString(std::string(session.error)),
      .sender                    = session.sender,
      .isSelfVerification        = session.is_self_verification,
      .isMultiDeviceVerification = session.is_multi_device_verification,
      .sasNumbers                = sasNumbers,
    };
}

MatrixUserDevice
fromRustUserDevice(const ::komai::rust::MatrixUserDevice &device)
{
    return MatrixUserDevice{
      .deviceId          = QString::fromStdString(std::string(device.device_id)),
      .displayName       = QString::fromStdString(std::string(device.display_name)),
      .verificationState = QString::fromStdString(std::string(device.verification_state)),
      .lastIp            = QString::fromStdString(std::string(device.last_seen_ip)),
      .lastTs            = device.last_seen_ts,
    };
}

MatrixUserVerificationState
fromRustUserVerificationState(const ::komai::rust::MatrixUserVerificationState &state)
{
    QVector<MatrixUserDevice> devices;
    devices.reserve(static_cast<int>(state.devices.size()));
    for (const auto &device : state.devices)
        devices.push_back(fromRustUserDevice(device));

    return MatrixUserVerificationState{
      .hasMasterKey = state.has_master_key,
      .userTrust    = QString::fromStdString(std::string(state.user_trust)),
      .devices      = devices,
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

MatrixRoomSummary
fromRustRoomSummary(const ::komai::rust::MatrixRoomSummary &room)
{
    QVector<QString> parentSpaceRoomIds;
    parentSpaceRoomIds.reserve(static_cast<int>(room.parent_space_room_ids.size()));
    for (const auto &value : room.parent_space_room_ids)
        parentSpaceRoomIds.push_back(QString::fromStdString(std::string(value)));

    return MatrixRoomSummary{
      .roomId      = QString::fromStdString(std::string(room.room_id)),
      .displayName = QString::fromStdString(std::string(room.display_name)),
      .avatarUrl   = matrix::normalizeMxcUri(QString::fromStdString(std::string(room.avatar_url))),
      .topic       = QString::fromStdString(std::string(room.topic)),
      .lastMessage = QString::fromStdString(std::string(room.last_message)),
      .lastMessageKind       = QString::fromStdString(std::string(room.last_message_kind)),
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

MatrixRoomSettings
fromRustRoomSettings(const ::komai::rust::MatrixRoomSettings &room)
{
    QVector<QString> allowedRoomIds;
    allowedRoomIds.reserve(static_cast<int>(room.allowed_room_ids.size()));
    for (const auto &value : room.allowed_room_ids)
        allowedRoomIds.push_back(QString::fromStdString(std::string(value)));

    QVector<QString> parentSpaceRoomIds;
    parentSpaceRoomIds.reserve(static_cast<int>(room.parent_space_room_ids.size()));
    for (const auto &value : room.parent_space_room_ids)
        parentSpaceRoomIds.push_back(QString::fromStdString(std::string(value)));

    return MatrixRoomSettings{
      .roomId    = QString::fromStdString(std::string(room.room_id)),
      .roomName  = QString::fromStdString(std::string(room.room_name)),
      .roomTopic = QString::fromStdString(std::string(room.room_topic)),
      .roomAvatarUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(room.room_avatar_url))),
      .roomVersion                = QString::fromStdString(std::string(room.room_version)),
      .memberCount                = room.member_count,
      .notifications              = room.notifications,
      .joinRule                   = QString::fromStdString(std::string(room.join_rule)),
      .historyVisibility          = QString::fromStdString(std::string(room.history_visibility)),
      .allowedRoomIds             = allowedRoomIds,
      .parentSpaceRoomIds         = parentSpaceRoomIds,
      .guestAccess                = room.guest_access,
      .isEncrypted                = room.is_encrypted,
      .canChangeName              = room.can_change_name,
      .canChangeTopic             = room.can_change_topic,
      .canChangeAvatar            = room.can_change_avatar,
      .canChangeJoinRules         = room.can_change_join_rules,
      .canChangeHistoryVisibility = room.can_change_history_visibility,
    };
}

MatrixRoomRedactionPermissions
fromRustRoomRedactionPermissions(const ::komai::rust::MatrixRoomRedactionPermissions &permissions)
{
    return MatrixRoomRedactionPermissions{
      .canRedactOwn   = permissions.can_redact_own,
      .canRedactOther = permissions.can_redact_other,
    };
}

MatrixReadReceiptEntry
fromRustReadReceiptEntry(const ::komai::rust::MatrixReadReceiptEntry &entry)
{
    return MatrixReadReceiptEntry{
      .userId      = QString::fromStdString(std::string(entry.user_id)),
      .displayName = QString::fromStdString(std::string(entry.display_name)),
      .avatarUrl   = matrix::normalizeMxcUri(QString::fromStdString(std::string(entry.avatar_url))),
      .timestamp   = entry.timestamp,
    };
}

MatrixTimelineItem
fromRustTimelineItem(const ::komai::rust::MatrixTimelineItem &item)
{
    QVariantList reactions;
    reactions.reserve(static_cast<qsizetype>(item.reactions.size()));
    for (const auto &reaction : item.reactions) {
        Reaction value;
        value.key_              = QString::fromStdString(std::string(reaction.key));
        value.users_            = QString::fromStdString(std::string(reaction.users));
        value.selfReactedEvent_ = QString::fromStdString(std::string(reaction.self_reacted_event));
        value.count_            = static_cast<int>(reaction.count);
        reactions.push_back(QVariant::fromValue(value));
    }

    return MatrixTimelineItem{
      .itemId            = QString::fromStdString(std::string(item.item_id)),
      .eventId           = QString::fromStdString(std::string(item.event_id)),
      .threadId          = QString::fromStdString(std::string(item.thread_id)),
      .senderId          = QString::fromStdString(std::string(item.sender_id)),
      .senderDisplayName = QString::fromStdString(std::string(item.sender_display_name)),
      .senderAvatarUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.sender_avatar_url))),
      .body                   = QString::fromStdString(std::string(item.body)),
      .replyEventId           = QString::fromStdString(std::string(item.reply_event_id)),
      .replySenderId          = QString::fromStdString(std::string(item.reply_sender_id)),
      .replySenderDisplayName = QString::fromStdString(std::string(item.reply_sender_display_name)),
      .replyBody              = QString::fromStdString(std::string(item.reply_body)),
      .reactions              = reactions,
      .reactionsSummary       = QString::fromStdString(std::string(item.reactions_summary)),
      .itemKind               = QString::fromStdString(std::string(item.item_kind)),
      .isEdited               = item.is_edited,
      .mediaUrl = matrix::normalizeMxcUri(QString::fromStdString(std::string(item.media_url))),
      .thumbnailUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.thumbnail_url))),
      .fileName             = QString::fromStdString(std::string(item.file_name)),
      .mimeType             = QString::fromStdString(std::string(item.mime_type)),
      .mediaWidth           = item.media_width,
      .mediaHeight          = item.media_height,
      .mediaDurationMs      = item.media_duration_ms,
      .mediaSizeBytes       = item.media_size_bytes,
      .mediaIsEncrypted     = item.media_is_encrypted,
      .thumbnailIsEncrypted = item.thumbnail_is_encrypted,
      .timestamp            = item.timestamp,
      .isOwn                = item.is_own,
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
        const auto normalizedProfileId = normalizeProfileId(profileId);
        auto result =
          ::komai::rust::matrix_start_restored_backend(normalizedProfileId.toStdString());
        return fromRustHandleInfo(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::logoutBackend(uint64_t handleId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_logout_backend(handleId);
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

std::optional<MatrixRecoveryStatus>
MatrixBackendRuntimeService::fetchRecoveryStatus(uint64_t handleId, QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_fetch_recovery_status(handleId);
        return fromRustRecoveryStatus(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixSetupRecoveryResult>
MatrixBackendRuntimeService::setupRecovery(uint64_t handleId,
                                           bool useSSSS,
                                           const QString &passphrase,
                                           bool encryptionBackupOnlineEnabled,
                                           QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_setup_recovery(
          handleId, useSSSS, passphrase.toStdString(), encryptionBackupOnlineEnabled);
        return fromRustSetupRecoveryResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::recoverEncryptionSecrets(uint64_t handleId,
                                                      const QString &keyOrPassphrase,
                                                      QString *errorOut)
{
    try {
        ::komai::rust::matrix_recover_encryption_secrets(handleId, keyOrPassphrase.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixResetEncryptionIdentityResult>
MatrixBackendRuntimeService::startResetEncryptionIdentity(uint64_t handleId, QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_start_reset_encryption_identity(handleId);
        return fromRustResetEncryptionIdentityResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::continueResetEncryptionIdentityWithPassword(uint64_t handleId,
                                                                         const QString &password,
                                                                         QString *errorOut)
{
    try {
        ::komai::rust::matrix_continue_reset_encryption_identity_with_password(
          handleId, password.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::continueResetEncryptionIdentityAfterApproval(uint64_t handleId,
                                                                          QString *errorOut)
{
    try {
        ::komai::rust::matrix_continue_reset_encryption_identity_after_approval(handleId);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::cancelResetEncryptionIdentity(uint64_t handleId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_cancel_reset_encryption_identity(handleId);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixDeviceSignOutResult>
MatrixBackendRuntimeService::startSignOutDevice(uint64_t handleId,
                                                const QString &deviceId,
                                                QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_start_sign_out_device(handleId, deviceId.toStdString());
        return fromRustDeviceSignOutResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::continueSignOutDeviceWithPassword(uint64_t handleId,
                                                               const QString &password,
                                                               QString *errorOut)
{
    try {
        ::komai::rust::matrix_continue_sign_out_device_with_password(handleId,
                                                                     password.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::renameDevice(uint64_t handleId,
                                          const QString &deviceId,
                                          const QString &displayName,
                                          QString *errorOut)
{
    try {
        ::komai::rust::matrix_rename_device(
          handleId, deviceId.toStdString(), displayName.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::startSelfVerification(uint64_t handleId, QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_start_self_verification(handleId);
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::startUserVerification(uint64_t handleId,
                                                   const QString &userId,
                                                   QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_start_user_verification(handleId, userId.toStdString());
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::startDeviceVerification(uint64_t handleId,
                                                     const QString &userId,
                                                     const QString &deviceId,
                                                     QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_start_device_verification(
          handleId, userId.toStdString(), deviceId.toStdString());
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::unverifyDevice(uint64_t handleId,
                                            const QString &userId,
                                            const QString &deviceId,
                                            QString *errorOut)
{
    try {
        ::komai::rust::matrix_unverify_device(
          handleId, userId.toStdString(), deviceId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::blockDevice(uint64_t handleId,
                                         const QString &userId,
                                         const QString &deviceId,
                                         QString *errorOut)
{
    try {
        ::komai::rust::matrix_block_device(handleId, userId.toStdString(), deviceId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unblockDevice(uint64_t handleId,
                                           const QString &userId,
                                           const QString &deviceId,
                                           QString *errorOut)
{
    try {
        ::komai::rust::matrix_unblock_device(
          handleId, userId.toStdString(), deviceId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixUserVerificationState>
MatrixBackendRuntimeService::fetchUserVerificationState(uint64_t handleId,
                                                        const QString &userId,
                                                        QString *errorOut)
{
    try {
        auto result =
          ::komai::rust::matrix_fetch_user_verification_state(handleId, userId.toStdString());
        return fromRustUserVerificationState(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<QString>>
MatrixBackendRuntimeService::takePendingVerificationFlowIds(uint64_t handleId, QString *errorOut)
{
    try {
        const auto result = ::komai::rust::matrix_take_pending_verification_flow_ids(handleId);
        QVector<QString> flowIds;
        flowIds.reserve(static_cast<int>(result.size()));
        for (const auto &flowId : result)
            flowIds.push_back(QString::fromStdString(std::string(flowId)));
        return flowIds;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::fetchVerificationSession(uint64_t handleId,
                                                      const QString &flowId,
                                                      QString *errorOut)
{
    try {
        auto result =
          ::komai::rust::matrix_fetch_verification_session(handleId, flowId.toStdString());
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::clearVerificationSession(uint64_t handleId,
                                                      const QString &flowId,
                                                      QString *errorOut)
{
    try {
        ::komai::rust::matrix_clear_verification_session(handleId, flowId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::advanceVerificationSession(uint64_t handleId,
                                                        const QString &flowId,
                                                        QString *errorOut)
{
    try {
        ::komai::rust::matrix_advance_verification_session(handleId, flowId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::cancelVerificationSession(uint64_t handleId,
                                                       const QString &flowId,
                                                       bool mismatch,
                                                       QString *errorOut)
{
    try {
        ::komai::rust::matrix_cancel_verification_session(handleId, flowId.toStdString(), mismatch);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixUserProfile>
MatrixBackendRuntimeService::fetchUserProfile(uint64_t handleId,
                                              const QString &userId,
                                              QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_fetch_user_profile(handleId, userId.toStdString());
        return fromRustUserProfile(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<MatrixDirectoryUser>>
MatrixBackendRuntimeService::searchUsers(uint64_t handleId,
                                         const QString &searchTerm,
                                         uint64_t limit,
                                         QString *errorOut)
{
    try {
        const auto result =
          ::komai::rust::matrix_search_users(handleId, searchTerm.toStdString(), limit);
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
MatrixBackendRuntimeService::fetchPublicRoomDirectoryPage(uint64_t handleId,
                                                          const QString &searchTerm,
                                                          uint64_t limit,
                                                          const QString &since,
                                                          const QString &server,
                                                          QString *errorOut)
{
    try {
        const auto result = ::komai::rust::matrix_fetch_public_room_directory_page(
          handleId, searchTerm.toStdString(), limit, since.toStdString(), server.toStdString());
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

bool
MatrixBackendRuntimeService::setOwnDisplayName(uint64_t handleId,
                                               const QString &displayName,
                                               QString *errorOut)
{
    try {
        ::komai::rust::matrix_set_own_display_name(handleId, displayName.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::uploadOwnAvatar(uint64_t handleId,
                                             const QString &filePath,
                                             const QString &mimeType,
                                             QString *errorOut)
{
    try {
        ::komai::rust::matrix_upload_own_avatar(
          handleId, filePath.toStdString(), mimeType.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::removeOwnAvatar(uint64_t handleId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_remove_own_avatar(handleId);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::ignoreUser(uint64_t handleId, const QString &userId, QString *errorOut)
{
    try {
        ::komai::rust::matrix_ignore_user(handleId, userId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unignoreUser(uint64_t handleId,
                                          const QString &userId,
                                          QString *errorOut)
{
    try {
        ::komai::rust::matrix_unignore_user(handleId, userId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
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

std::optional<MatrixRoomSettings>
MatrixBackendRuntimeService::fetchRoomSettings(uint64_t handleId,
                                               const QString &roomId,
                                               QString *errorOut)
{
    try {
        auto result = ::komai::rust::matrix_fetch_room_settings(handleId, roomId.toStdString());
        return fromRustRoomSettings(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixRoomRedactionPermissions>
MatrixBackendRuntimeService::fetchRoomRedactionPermissions(uint64_t handleId,
                                                           const QString &roomId,
                                                           QString *errorOut)
{
    try {
        auto result =
          ::komai::rust::matrix_fetch_room_redaction_permissions(handleId, roomId.toStdString());
        return fromRustRoomRedactionPermissions(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::setRoomNotificationMode(uint64_t handleId,
                                                     const QString &roomId,
                                                     int mode,
                                                     QString *errorOut)
{
    try {
        ::komai::rust::matrix_set_room_notification_mode(handleId, roomId.toStdString(), mode);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setRoomName(uint64_t handleId,
                                         const QString &roomId,
                                         const QString &name,
                                         QString *errorOut)
{
    try {
        ::komai::rust::matrix_set_room_name(handleId, roomId.toStdString(), name.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setRoomTopic(uint64_t handleId,
                                          const QString &roomId,
                                          const QString &topic,
                                          QString *errorOut)
{
    try {
        ::komai::rust::matrix_set_room_topic(handleId, roomId.toStdString(), topic.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::uploadRoomAvatar(uint64_t handleId,
                                              const QString &roomId,
                                              const QString &filePath,
                                              const QString &mimeType,
                                              int width,
                                              int height,
                                              QString *errorOut)
{
    try {
        ::komai::rust::matrix_upload_room_avatar(handleId,
                                                 roomId.toStdString(),
                                                 filePath.toStdString(),
                                                 mimeType.toStdString(),
                                                 width,
                                                 height);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::removeRoomAvatar(uint64_t handleId,
                                              const QString &roomId,
                                              QString *errorOut)
{
    try {
        ::komai::rust::matrix_remove_room_avatar(handleId, roomId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::enableRoomEncryption(uint64_t handleId,
                                                  const QString &roomId,
                                                  QString *errorOut)
{
    try {
        ::komai::rust::matrix_enable_room_encryption(handleId, roomId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setRoomHistoryVisibility(uint64_t handleId,
                                                      const QString &roomId,
                                                      const QString &historyVisibility,
                                                      QString *errorOut)
{
    try {
        ::komai::rust::matrix_set_room_history_visibility(
          handleId, roomId.toStdString(), historyVisibility.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setRoomAccessRules(uint64_t handleId,
                                                const QString &roomId,
                                                const QString &joinRule,
                                                bool guestAccess,
                                                const QVector<QString> &allowedRoomIds,
                                                QString *errorOut)
{
    try {
        ::komai::rust::matrix_set_room_access_rules(handleId,
                                                    roomId.toStdString(),
                                                    joinRule.toStdString(),
                                                    guestAccess,
                                                    toRustStringVec(allowedRoomIds));
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
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

bool
MatrixBackendRuntimeService::sendRoomReplyMessage(uint64_t handleId,
                                                  const QString &roomId,
                                                  const QString &repliedToEventId,
                                                  const QString &body,
                                                  const QString &formattedHtml,
                                                  const QString &messageKind,
                                                  QString *errorOut)
{
    try {
        ::komai::rust::matrix_send_room_reply_message(handleId,
                                                      roomId.toStdString(),
                                                      repliedToEventId.toStdString(),
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

bool
MatrixBackendRuntimeService::sendRoomEditMessage(uint64_t handleId,
                                                 const QString &roomId,
                                                 const QString &targetEventId,
                                                 const QString &body,
                                                 const QString &formattedHtml,
                                                 const QString &messageKind,
                                                 QString *errorOut)
{
    try {
        ::komai::rust::matrix_send_room_edit_message(handleId,
                                                     roomId.toStdString(),
                                                     targetEventId.toStdString(),
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

bool
MatrixBackendRuntimeService::toggleRoomReaction(uint64_t handleId,
                                                const QString &roomId,
                                                const QString &eventId,
                                                const QString &reactionKey,
                                                QString *errorOut)
{
    try {
        ::komai::rust::matrix_toggle_room_reaction(
          handleId, roomId.toStdString(), eventId.toStdString(), reactionKey.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::redactRoomEvent(uint64_t handleId,
                                             const QString &roomId,
                                             const QString &eventId,
                                             const QString &reason,
                                             QString *errorOut)
{
    try {
        ::komai::rust::matrix_redact_room_event(
          handleId, roomId.toStdString(), eventId.toStdString(), reason.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::markRoomEventAsRead(uint64_t handleId,
                                                 const QString &roomId,
                                                 const QString &eventId,
                                                 QString *errorOut)
{
    try {
        ::komai::rust::matrix_mark_room_event_as_read(
          handleId, roomId.toStdString(), eventId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::reportRoomEvent(uint64_t handleId,
                                             const QString &roomId,
                                             const QString &eventId,
                                             const QString &reason,
                                             int score,
                                             QString *errorOut)
{
    try {
        ::komai::rust::matrix_report_room_event(
          handleId, roomId.toStdString(), eventId.toStdString(), reason.toStdString(), score);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QStringList>
MatrixBackendRuntimeService::fetchRoomPinnedEventIds(uint64_t handleId,
                                                     const QString &roomId,
                                                     QString *errorOut)
{
    try {
        const auto result =
          ::komai::rust::matrix_fetch_room_pinned_event_ids(handleId, roomId.toStdString());
        QStringList pinnedEventIds;
        pinnedEventIds.reserve(static_cast<qsizetype>(result.size()));
        for (const auto &eventId : result)
            pinnedEventIds.push_back(QString::fromStdString(std::string(eventId)));
        return pinnedEventIds;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::pinRoomEvent(uint64_t handleId,
                                          const QString &roomId,
                                          const QString &eventId,
                                          QString *errorOut)
{
    try {
        ::komai::rust::matrix_pin_room_event(handleId, roomId.toStdString(), eventId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unpinRoomEvent(uint64_t handleId,
                                            const QString &roomId,
                                            const QString &eventId,
                                            QString *errorOut)
{
    try {
        ::komai::rust::matrix_unpin_room_event(
          handleId, roomId.toStdString(), eventId.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QString>
MatrixBackendRuntimeService::fetchActiveRoomRawEventJson(uint64_t handleId,
                                                         const QString &roomId,
                                                         const QString &eventId,
                                                         QString *errorOut)
{
    try {
        return QString::fromStdString(
          std::string(::komai::rust::matrix_fetch_active_room_raw_event_json(
            handleId, roomId.toStdString(), eventId.toStdString())));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<MatrixReadReceiptEntry>>
MatrixBackendRuntimeService::fetchRoomReadReceipts(uint64_t handleId,
                                                   const QString &roomId,
                                                   const QString &eventId,
                                                   QString *errorOut)
{
    try {
        const auto result = ::komai::rust::matrix_fetch_room_read_receipts(
          handleId, roomId.toStdString(), eventId.toStdString());
        QVector<MatrixReadReceiptEntry> receipts;
        receipts.reserve(static_cast<int>(result.size()));
        for (const auto &entry : result)
            receipts.push_back(fromRustReadReceiptEntry(entry));
        return receipts;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::sendRoomAttachment(uint64_t handleId,
                                                const QString &roomId,
                                                const QString &filePath,
                                                const QString &filename,
                                                const QString &caption,
                                                const QString &replyEventId,
                                                const QString &mimeType,
                                                QString *errorOut)
{
    try {
        ::komai::rust::matrix_send_room_attachment(handleId,
                                                   roomId.toStdString(),
                                                   filePath.toStdString(),
                                                   filename.toStdString(),
                                                   caption.toStdString(),
                                                   replyEventId.toStdString(),
                                                   mimeType.toStdString());
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QByteArray>
MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(uint64_t handleId,
                                                                 const QString &itemId,
                                                                 int width,
                                                                 int height,
                                                                 bool crop,
                                                                 QString *errorOut)
{
    try {
        const auto result = ::komai::rust::matrix_fetch_active_room_timeline_media_content(
          handleId, itemId.toStdString(), width, height, crop);
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
