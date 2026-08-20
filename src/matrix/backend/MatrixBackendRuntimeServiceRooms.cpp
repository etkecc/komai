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

::rust::Vec<::rust::String>
toRustStringVec(const QVector<QString> &values)
{
    ::rust::Vec<::rust::String> rustValues;
    for (const auto &value : values)
        rustValues.push_back(value.toStdString());
    return rustValues;
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
      .canChangeEncryption        = room.can_change_encryption,
      .canUpgradeRoom             = room.can_upgrade_room,
    };
}

MatrixRoomVersionsCapability
fromRustRoomVersionsCapability(const ::komai::rust::MatrixRoomVersionsCapability &cap)
{
    QVector<QString> stable;
    stable.reserve(static_cast<int>(cap.stable.size()));
    for (const auto &value : cap.stable)
        stable.push_back(QString::fromStdString(std::string(value)));

    return MatrixRoomVersionsCapability{
      .defaultVersion = QString::fromStdString(std::string(cap.default_version)),
      .stableVersions = stable,
    };
}

MatrixRoomAliases
fromRustRoomAliases(const ::komai::rust::MatrixRoomAliases &aliases)
{
    QVector<QString> altAliases;
    altAliases.reserve(static_cast<int>(aliases.alt_aliases.size()));
    for (const auto &value : aliases.alt_aliases)
        altAliases.push_back(QString::fromStdString(std::string(value)));

    QVector<QString> publishedAliases;
    publishedAliases.reserve(static_cast<int>(aliases.published_aliases.size()));
    for (const auto &value : aliases.published_aliases)
        publishedAliases.push_back(QString::fromStdString(std::string(value)));

    return MatrixRoomAliases{
      .canonicalAlias   = QString::fromStdString(std::string(aliases.canonical_alias)),
      .altAliases       = altAliases,
      .publishedAliases = publishedAliases,
    };
}

::komai::rust::MatrixRoomAliases
toRustRoomAliases(const MatrixRoomAliases &aliases)
{
    return ::komai::rust::MatrixRoomAliases{
      .canonical_alias   = aliases.canonicalAlias.toStdString(),
      .alt_aliases       = toRustStringVec(aliases.altAliases),
      .published_aliases = toRustStringVec(aliases.publishedAliases),
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

MatrixRoomMember
fromRustRoomMember(const ::komai::rust::MatrixRoomMember &member)
{
    return MatrixRoomMember{
      .userId      = QString::fromStdString(std::string(member.user_id)),
      .displayName = QString::fromStdString(std::string(member.display_name)),
      .avatarUrl  = matrix::normalizeMxcUri(QString::fromStdString(std::string(member.avatar_url))),
      .powerLevel = member.power_level,
      .isInvited  = member.is_invited,
    };
}

MatrixPowerLevelEntry
fromRustPowerLevelEntry(const ::komai::rust::MatrixPowerLevelEntry &entry)
{
    return MatrixPowerLevelEntry{
      .key   = QString::fromStdString(std::string(entry.key)),
      .level = entry.level,
    };
}

MatrixRoomPowerLevels
fromRustRoomPowerLevels(const ::komai::rust::MatrixRoomPowerLevels &powerLevels)
{
    QVector<QString> creators;
    creators.reserve(static_cast<int>(powerLevels.creators.size()));
    for (const auto &creator : powerLevels.creators)
        creators.push_back(QString::fromStdString(std::string(creator)));

    QVector<MatrixPowerLevelEntry> events;
    events.reserve(static_cast<int>(powerLevels.events.size()));
    for (const auto &entry : powerLevels.events)
        events.push_back(fromRustPowerLevelEntry(entry));

    QVector<MatrixPowerLevelEntry> users;
    users.reserve(static_cast<int>(powerLevels.users.size()));
    for (const auto &entry : powerLevels.users)
        users.push_back(fromRustPowerLevelEntry(entry));

    return MatrixRoomPowerLevels{
      .roomVersion   = QString::fromStdString(std::string(powerLevels.room_version)),
      .creators      = creators,
      .events        = events,
      .users         = users,
      .ban           = powerLevels.ban,
      .eventsDefault = powerLevels.events_default,
      .invite        = powerLevels.invite,
      .kick          = powerLevels.kick,
      .redact        = powerLevels.redact,
      .stateDefault  = powerLevels.state_default,
      .usersDefault  = powerLevels.users_default,
    };
}

::komai::rust::MatrixPowerLevelEntry
toRustPowerLevelEntry(const MatrixPowerLevelEntry &entry)
{
    return ::komai::rust::MatrixPowerLevelEntry{
      .key   = entry.key.toStdString(),
      .level = entry.level,
    };
}

::komai::rust::MatrixRoomPowerLevels
toRustRoomPowerLevels(const MatrixRoomPowerLevels &powerLevels)
{
    ::rust::Vec<::rust::String> creators;
    for (const auto &creator : powerLevels.creators)
        creators.push_back(creator.toStdString());

    ::rust::Vec<::komai::rust::MatrixPowerLevelEntry> events;
    for (const auto &entry : powerLevels.events)
        events.push_back(toRustPowerLevelEntry(entry));

    ::rust::Vec<::komai::rust::MatrixPowerLevelEntry> users;
    for (const auto &entry : powerLevels.users)
        users.push_back(toRustPowerLevelEntry(entry));

    return ::komai::rust::MatrixRoomPowerLevels{
      .room_version   = powerLevels.roomVersion.toStdString(),
      .creators       = std::move(creators),
      .events         = std::move(events),
      .users          = std::move(users),
      .ban            = powerLevels.ban,
      .events_default = powerLevels.eventsDefault,
      .invite         = powerLevels.invite,
      .kick           = powerLevels.kick,
      .redact         = powerLevels.redact,
      .state_default  = powerLevels.stateDefault,
      .users_default  = powerLevels.usersDefault,
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

} // anonymous namespace

MatrixJoinRoomResult
MatrixBackendRuntimeService::joinRoom(matrix_backend::BlockingCallContext context,
                                      uint64_t handleId,
                                      const QString &roomIdOrAlias,
                                      const QVector<QString> &via,
                                      const QString &reason)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_join_room", [context, handleId, &roomIdOrAlias, &via, &reason]() {
              return ::komai::rust::matrix_join_room(matrix_backend::toRustBlockingContext(context),
                                                     handleId,
                                                     roomIdOrAlias.toStdString(),
                                                     toRustStringVec(via),
                                                     reason.toStdString());
          });
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
MatrixBackendRuntimeService::knockRoom(matrix_backend::BlockingCallContext context,
                                       uint64_t handleId,
                                       const QString &roomIdOrAlias,
                                       const QVector<QString> &via,
                                       const QString &reason,
                                       QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_knock_room", [context, handleId, &roomIdOrAlias, &via, &reason]() {
              return ::komai::rust::matrix_knock_room(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomIdOrAlias.toStdString(),
                toRustStringVec(via),
                reason.toStdString());
          });
        return QString::fromStdString(std::string(result));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QString>
MatrixBackendRuntimeService::createRoom(matrix_backend::BlockingCallContext context,
                                        uint64_t handleId,
                                        const MatrixCreateRoomRequest &request,
                                        QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_create_room", [context, handleId, &request]() {
              return ::komai::rust::matrix_create_room(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                request.name.toStdString(),
                request.topic.toStdString(),
                request.roomAliasLocalpart.toStdString(),
                toRustStringVec(request.inviteUserIds),
                toRustCreateRoomPreset(request.preset),
                request.isDirect,
                request.isEncrypted,
                request.isSpace,
                request.isPublic,
                request.roomVersion.toStdString(),
                request.powerLevelContentOverrideJson.toStdString(),
                request.initialStateJson.toStdString(),
                request.creationContentJson.toStdString());
          });
        return QString::fromStdString(std::string(result));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::leaveRoom(matrix_backend::BlockingCallContext context,
                                       uint64_t handleId,
                                       const QString &roomId,
                                       const QString &reason,
                                       QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_leave_room", [context, handleId, &roomId, &reason]() {
            ::komai::rust::matrix_leave_room(matrix_backend::toRustBlockingContext(context),
                                             handleId,
                                             roomId.toStdString(),
                                             reason.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::toggleRoomTag(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           const QString &roomId,
                                           const QString &tag,
                                           bool enabled,
                                           QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_toggle_room_tag", [context, handleId, &roomId, &tag, enabled]() {
              ::komai::rust::matrix_toggle_room_tag(matrix_backend::toRustBlockingContext(context),
                                                    handleId,
                                                    roomId.toStdString(),
                                                    tag.toStdString(),
                                                    enabled);
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setRoomIsDirect(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             bool isDirect,
                                             QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_set_room_is_direct",
                                [context, handleId, &roomId, isDirect]() {
                                    ::komai::rust::matrix_set_room_is_direct(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      isDirect);
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::inviteUser(matrix_backend::BlockingCallContext context,
                                        uint64_t handleId,
                                        const QString &roomId,
                                        const QString &userId,
                                        const QString &reason,
                                        QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_invite_user", [context, handleId, &roomId, &userId, &reason]() {
              ::komai::rust::matrix_invite_user(matrix_backend::toRustBlockingContext(context),
                                                handleId,
                                                roomId.toStdString(),
                                                userId.toStdString(),
                                                reason.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::kickUser(matrix_backend::BlockingCallContext context,
                                      uint64_t handleId,
                                      const QString &roomId,
                                      const QString &userId,
                                      const QString &reason,
                                      QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_kick_user", [context, handleId, &roomId, &userId, &reason]() {
              ::komai::rust::matrix_kick_user(matrix_backend::toRustBlockingContext(context),
                                              handleId,
                                              roomId.toStdString(),
                                              userId.toStdString(),
                                              reason.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::banUser(matrix_backend::BlockingCallContext context,
                                     uint64_t handleId,
                                     const QString &roomId,
                                     const QString &userId,
                                     const QString &reason,
                                     QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_ban_user", [context, handleId, &roomId, &userId, &reason]() {
              ::komai::rust::matrix_ban_user(matrix_backend::toRustBlockingContext(context),
                                             handleId,
                                             roomId.toStdString(),
                                             userId.toStdString(),
                                             reason.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unbanUser(matrix_backend::BlockingCallContext context,
                                       uint64_t handleId,
                                       const QString &roomId,
                                       const QString &userId,
                                       const QString &reason,
                                       QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_unban_user", [context, handleId, &roomId, &userId, &reason]() {
              ::komai::rust::matrix_unban_user(matrix_backend::toRustBlockingContext(context),
                                               handleId,
                                               roomId.toStdString(),
                                               userId.toStdString(),
                                               reason.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QString>
MatrixBackendRuntimeService::upgradeRoom(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &roomId,
                                         const QString &newVersion,
                                         const QStringList &additionalCreators,
                                         QString *errorOut)
{
    try {
        std::vector<std::string> rustCreators;
        rustCreators.reserve(static_cast<size_t>(additionalCreators.size()));
        for (const auto &creator : additionalCreators)
            rustCreators.push_back(creator.toStdString());

        auto result = invokeRuntimeWorkerCall(
          "matrix_upgrade_room", [context, handleId, &roomId, &newVersion, &rustCreators]() {
              ::rust::Vec<::rust::String> creatorsForRust;
              creatorsForRust.reserve(rustCreators.size());
              for (const auto &c : rustCreators)
                  creatorsForRust.push_back(c);

              return ::komai::rust::matrix_upgrade_room(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                newVersion.toStdString(),
                creatorsForRust);
          });
        return QString::fromStdString(std::string(result));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::setUserPowerLevel(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &roomId,
                                               const QString &userId,
                                               int64_t powerLevel,
                                               QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_set_user_power_level",
                                [context, handleId, &roomId, &userId, powerLevel]() {
                                    ::komai::rust::matrix_set_user_power_level(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      userId.toStdString(),
                                      powerLevel);
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixRoomSettings>
MatrixBackendRuntimeService::fetchRoomSettings(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &roomId,
                                               QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_fetch_room_settings", [context, handleId, roomId]() {
              return ::komai::rust::matrix_fetch_room_settings(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });
        return fromRustRoomSettings(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixRoomVersionsCapability>
MatrixBackendRuntimeService::fetchRoomVersionsCapability(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_room_versions_capability", [context, handleId]() {
              return ::komai::rust::matrix_fetch_room_versions_capability(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return fromRustRoomVersionsCapability(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixRoomAliases>
MatrixBackendRuntimeService::fetchRoomAliases(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &roomId,
                                              QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_room_aliases", [context, handleId, roomId]() {
              return ::komai::rust::matrix_fetch_room_aliases(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });
        return fromRustRoomAliases(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::applyRoomAliases(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &roomId,
                                              const MatrixRoomAliases &aliases,
                                              QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_apply_room_aliases",
                                [context, handleId, roomId, aliases]() {
                                    ::komai::rust::matrix_apply_room_aliases(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      toRustRoomAliases(aliases));
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixRoomRedactionPermissions>
MatrixBackendRuntimeService::fetchRoomRedactionPermissions(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &roomId,
  QString *errorOut)
{
    try {
        auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_room_redaction_permissions",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, context]() {
              return ::komai::rust::matrix_fetch_room_redaction_permissions(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });
        return fromRustRoomRedactionPermissions(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<MatrixRoomMember>>
MatrixBackendRuntimeService::fetchRoomMembers(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &roomId,
                                              QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_room_members", [context, handleId, roomId]() {
              return ::komai::rust::matrix_fetch_room_members(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });
        QVector<MatrixRoomMember> members;
        members.reserve(static_cast<int>(result.size()));
        for (const auto &member : result)
            members.push_back(fromRustRoomMember(member));
        return members;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixRoomPowerLevels>
MatrixBackendRuntimeService::fetchRoomPowerLevels(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &roomId,
                                                  QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_room_power_levels", [context, handleId, roomId]() {
              return ::komai::rust::matrix_fetch_room_power_levels(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });
        return fromRustRoomPowerLevels(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::applyRoomPowerLevels(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &roomId,
                                                  const MatrixRoomPowerLevels &powerLevels,
                                                  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_apply_room_power_levels",
                                [context, handleId, roomId, powerLevels]() {
                                    ::komai::rust::matrix_apply_room_power_levels(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      toRustRoomPowerLevels(powerLevels));
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QVector<MatrixChildSpaceEntry>>
MatrixBackendRuntimeService::fetchRoomChildSpaces(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &roomId,
                                                  QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_room_child_spaces", [context, handleId, roomId]() {
              return ::komai::rust::matrix_fetch_room_child_spaces(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });

        QVector<MatrixChildSpaceEntry> entries;
        entries.reserve(static_cast<int>(result.size()));
        for (const auto &entry : result) {
            entries.push_back(MatrixChildSpaceEntry{
              .roomId      = QString::fromStdString(std::string(entry.room_id)),
              .displayName = QString::fromStdString(std::string(entry.display_name)),
              .avatarUrl   = QString::fromStdString(std::string(entry.avatar_url)),
              .powerLevels = fromRustRoomPowerLevels(entry.power_levels),
            });
        }
        return entries;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::setRoomNotificationMode(matrix_backend::BlockingCallContext context,
                                                     uint64_t handleId,
                                                     const QString &roomId,
                                                     int mode,
                                                     QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_set_room_notification_mode",
                                [context, handleId, roomId, mode]() {
                                    ::komai::rust::matrix_set_room_notification_mode(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      mode);
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setRoomName(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &roomId,
                                         const QString &name,
                                         QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_set_room_name", [context, handleId, roomId, name]() {
            ::komai::rust::matrix_set_room_name(matrix_backend::toRustBlockingContext(context),
                                                handleId,
                                                roomId.toStdString(),
                                                name.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setRoomTopic(matrix_backend::BlockingCallContext context,
                                          uint64_t handleId,
                                          const QString &roomId,
                                          const QString &topic,
                                          QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_set_room_topic", [context, handleId, roomId, topic]() {
            ::komai::rust::matrix_set_room_topic(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 roomId.toStdString(),
                                                 topic.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixRoomStateEventResult>
MatrixBackendRuntimeService::fetchRoomStateEvent(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 const QString &roomId,
                                                 const QString &eventType,
                                                 const QString &stateKey,
                                                 QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_room_state_event", [context, handleId, roomId, eventType, stateKey]() {
              return ::komai::rust::matrix_fetch_room_state_event(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventType.toStdString(),
                stateKey.toStdString());
          });

        return MatrixRoomStateEventResult{
          .exists      = result.exists,
          .contentJson = QString::fromStdString(std::string(result.content_json)),
        };
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QString>
MatrixBackendRuntimeService::sendRoomStateEvent(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId,
                                                const QString &eventType,
                                                const QString &stateKey,
                                                const QString &contentJson,
                                                QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_send_room_state_event",
                                  [context, handleId, roomId, eventType, stateKey, contentJson]() {
                                      return ::komai::rust::matrix_send_room_state_event(
                                        matrix_backend::toRustBlockingContext(context),
                                        handleId,
                                        roomId.toStdString(),
                                        eventType.toStdString(),
                                        stateKey.toStdString(),
                                        contentJson.toStdString());
                                  });
        return QString::fromStdString(std::string(result));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

QString
MatrixBackendRuntimeService::uploadRoomAvatar(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &roomId,
                                              const QString &filePath,
                                              const QString &mimeType,
                                              int width,
                                              int height,
                                              QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_upload_room_avatar",
                                  [context, handleId, roomId, filePath, mimeType, width, height]() {
                                      return ::komai::rust::matrix_upload_room_avatar(
                                        matrix_backend::toRustBlockingContext(context),
                                        handleId,
                                        roomId.toStdString(),
                                        filePath.toStdString(),
                                        mimeType.toStdString(),
                                        width,
                                        height);
                                  });
        return QString::fromStdString(std::string(result));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return {};
    }
}

bool
MatrixBackendRuntimeService::removeRoomAvatar(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &roomId,
                                              QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_remove_room_avatar", [context, handleId, roomId]() {
            ::komai::rust::matrix_remove_room_avatar(
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
MatrixBackendRuntimeService::enableRoomEncryption(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &roomId,
                                                  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_enable_room_encryption", [context, handleId, roomId]() {
            ::komai::rust::matrix_enable_room_encryption(
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
MatrixBackendRuntimeService::setRoomHistoryVisibility(matrix_backend::BlockingCallContext context,
                                                      uint64_t handleId,
                                                      const QString &roomId,
                                                      const QString &historyVisibility,
                                                      QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_set_room_history_visibility",
                                [context, handleId, roomId, historyVisibility]() {
                                    ::komai::rust::matrix_set_room_history_visibility(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      historyVisibility.toStdString());
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::setRoomAccessRules(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId,
                                                const QString &joinRule,
                                                bool guestAccess,
                                                const QVector<QString> &allowedRoomIds,
                                                QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_set_room_access_rules",
          [context, handleId, roomId, joinRule, guestAccess, allowedRoomIds]() {
              ::komai::rust::matrix_set_room_access_rules(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                joinRule.toStdString(),
                guestAccess,
                toRustStringVec(allowedRoomIds));
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

} // namespace komai
