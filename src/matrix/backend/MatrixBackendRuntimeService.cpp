// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/backend/MatrixBackendRuntimeService.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QStringList>

#include "komai-rust-cxxbridge/ffi.h"
#include "logging/Logging.h"
#include "matrix/MatrixMediaUri.h"
#include "matrix/backend/MatrixBackendBridge.h"
#include "matrix/backend/MatrixBlockingCall.h"
#include "matrix/backend/MatrixFfiBlockingContext.h"
#include "profile/ProfileId.h"

namespace komai {

namespace {
QHash<uint64_t, QVector<MatrixRoomSummary>> cachedRoomListSnapshots;

bool
isTruthyEnvValue(const QByteArray &value)
{
    const auto normalized = value.trimmed().toLower();
    return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
}

bool
roomSwitchPerfEnabled()
{
    return isTruthyEnvValue(qgetenv("KOMAI_ROOM_SWITCH_PERF")) ||
           isTruthyEnvValue(qgetenv("KOMAI_PERF_ROOM_SWITCH"));
}

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

template<typename Func>
auto
invokeRuntimeWorkerCall(const char *operation, Func &&func)
{
    return matrix_backend::invokeBlockingCall(
      operation,
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      std::forward<Func>(func));
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
      .lastMessage = QString::fromStdString(std::string(room.last_message)),
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

    QStringList specialEffectNames;
    specialEffectNames.reserve(static_cast<qsizetype>(item.special_effect_names.size()));
    for (const auto &effectName : item.special_effect_names)
        specialEffectNames.push_back(QString::fromStdString(std::string(effectName)));

    return MatrixTimelineItem{
      .itemId            = QString::fromStdString(std::string(item.item_id)),
      .eventId           = QString::fromStdString(std::string(item.event_id)),
      .deliveryState     = QString::fromStdString(std::string(item.delivery_state)),
      .threadId          = QString::fromStdString(std::string(item.thread_id)),
      .senderId          = QString::fromStdString(std::string(item.sender_id)),
      .senderDisplayName = QString::fromStdString(std::string(item.sender_display_name)),
      .senderAvatarUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.sender_avatar_url))),
      .body                   = QString::fromStdString(std::string(item.body)),
      .formattedBody          = QString::fromStdString(std::string(item.formatted_body)),
      .replyEventId           = QString::fromStdString(std::string(item.reply_event_id)),
      .replySenderId          = QString::fromStdString(std::string(item.reply_sender_id)),
      .replySenderDisplayName = QString::fromStdString(std::string(item.reply_sender_display_name)),
      .replyItemKind          = QString::fromStdString(std::string(item.reply_item_kind)),
      .replyMatrixEventType   = QString::fromStdString(std::string(item.reply_matrix_event_type)),
      .replyBody              = QString::fromStdString(std::string(item.reply_body)),
      .replyFormattedBody     = QString::fromStdString(std::string(item.reply_formatted_body)),
      .replyMediaUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.reply_media_url))),
      .replyThumbnailUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.reply_thumbnail_url))),
      .replyFileName        = QString::fromStdString(std::string(item.reply_file_name)),
      .replyMimeType        = QString::fromStdString(std::string(item.reply_mime_type)),
      .replyMediaWidth      = item.reply_media_width,
      .replyMediaHeight     = item.reply_media_height,
      .replyMediaDurationMs = item.reply_media_duration_ms,
      .replyMediaSizeBytes  = item.reply_media_size_bytes,
      .replyBlurhash        = QString::fromStdString(std::string(item.reply_blurhash)),
      .reactions            = reactions,
      .reactionsSummary     = QString::fromStdString(std::string(item.reactions_summary)),
      .specialEffectNames   = specialEffectNames,
      .itemKind             = QString::fromStdString(std::string(item.item_kind)),
      .matrixEventType      = QString::fromStdString(std::string(item.matrix_event_type)),
      .isEdited             = item.is_edited,
      .mediaUrl = matrix::normalizeMxcUri(QString::fromStdString(std::string(item.media_url))),
      .thumbnailUrl =
        matrix::normalizeMxcUri(QString::fromStdString(std::string(item.thumbnail_url))),
      .fileName                  = QString::fromStdString(std::string(item.file_name)),
      .mimeType                  = QString::fromStdString(std::string(item.mime_type)),
      .mediaWidth                = item.media_width,
      .mediaHeight               = item.media_height,
      .mediaDurationMs           = item.media_duration_ms,
      .mediaSizeBytes            = item.media_size_bytes,
      .blurhash                  = QString::fromStdString(std::string(item.blurhash)),
      .mediaIsEncrypted          = item.media_is_encrypted,
      .thumbnailIsEncrypted      = item.thumbnail_is_encrypted,
      .timestamp                 = item.timestamp,
      .isOwn                     = item.is_own,
      .cachedType                = 0,
      .cachedEmojiOnlyCount      = 0,
      .cachedDay                 = 0,
      .cachedStatus              = 0,
      .cachedIsStateEvent        = false,
      .cachedIsEncrypted         = false,
      .cachedIsEditable          = false,
      .cachedProportionalH       = 0.0,
      .cachedFormattedBody       = QString(),
      .cachedFormattedStateEvent = QString(),
      .cachedStateEventIcon      = QString(),
      .cachedFilesize            = QString(),
      .cachedFilename            = QString(),
      .cachedFileTypeIcon        = QString(),
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
                request.isPublic);
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

std::optional<MatrixRecoveryStatus>
MatrixBackendRuntimeService::fetchRecoveryStatus(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_fetch_recovery_status", [context, handleId]() {
              return ::komai::rust::matrix_fetch_recovery_status(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return fromRustRecoveryStatus(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixSetupRecoveryResult>
MatrixBackendRuntimeService::setupRecovery(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           bool useSSSS,
                                           const QString &passphrase,
                                           bool encryptionBackupOnlineEnabled,
                                           QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall(
          "matrix_setup_recovery",
          [context, handleId, useSSSS, passphrase, encryptionBackupOnlineEnabled]() {
              return ::komai::rust::matrix_setup_recovery(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                useSSSS,
                passphrase.toStdString(),
                encryptionBackupOnlineEnabled);
          });
        return fromRustSetupRecoveryResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::recoverEncryptionSecrets(matrix_backend::BlockingCallContext context,
                                                      uint64_t handleId,
                                                      const QString &keyOrPassphrase,
                                                      QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_recover_encryption_secrets",
                                [context, handleId, keyOrPassphrase]() {
                                    ::komai::rust::matrix_recover_encryption_secrets(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      keyOrPassphrase.toStdString());
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixResetEncryptionIdentityResult>
MatrixBackendRuntimeService::startResetEncryptionIdentity(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_start_reset_encryption_identity", [context, handleId]() {
              return ::komai::rust::matrix_start_reset_encryption_identity(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return fromRustResetEncryptionIdentityResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::continueResetEncryptionIdentityWithPassword(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &password,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_continue_reset_encryption_identity_with_password",
          [context, handleId, password]() {
              ::komai::rust::matrix_continue_reset_encryption_identity_with_password(
                matrix_backend::toRustBlockingContext(context), handleId, password.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::continueResetEncryptionIdentityAfterApproval(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_continue_reset_encryption_identity_after_approval", [context, handleId]() {
              ::komai::rust::matrix_continue_reset_encryption_identity_after_approval(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::cancelResetEncryptionIdentity(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_cancel_reset_encryption_identity", [context, handleId]() {
            ::komai::rust::matrix_cancel_reset_encryption_identity(
              matrix_backend::toRustBlockingContext(context), handleId);
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixDeviceSignOutResult>
MatrixBackendRuntimeService::startSignOutDevice(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &deviceId,
                                                QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_start_sign_out_device", [context, handleId, deviceId]() {
              return ::komai::rust::matrix_start_sign_out_device(
                matrix_backend::toRustBlockingContext(context), handleId, deviceId.toStdString());
          });
        return fromRustDeviceSignOutResult(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::continueSignOutDeviceWithPassword(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &password,
  QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_continue_sign_out_device_with_password", [context, handleId, password]() {
              ::komai::rust::matrix_continue_sign_out_device_with_password(
                matrix_backend::toRustBlockingContext(context), handleId, password.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::renameDevice(matrix_backend::BlockingCallContext context,
                                          uint64_t handleId,
                                          const QString &deviceId,
                                          const QString &displayName,
                                          QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_rename_device", [context, handleId, deviceId, displayName]() {
              ::komai::rust::matrix_rename_device(matrix_backend::toRustBlockingContext(context),
                                                  handleId,
                                                  deviceId.toStdString(),
                                                  displayName.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::startSelfVerification(matrix_backend::BlockingCallContext context,
                                                   uint64_t handleId,
                                                   QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_start_self_verification", [context, handleId]() {
              return ::komai::rust::matrix_start_self_verification(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::startUserVerification(matrix_backend::BlockingCallContext context,
                                                   uint64_t handleId,
                                                   const QString &userId,
                                                   QString *errorOut)
{
    try {
        auto result =
          invokeRuntimeWorkerCall("matrix_start_user_verification", [context, handleId, userId]() {
              return ::komai::rust::matrix_start_user_verification(
                matrix_backend::toRustBlockingContext(context), handleId, userId.toStdString());
          });
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixVerificationSession>
MatrixBackendRuntimeService::startDeviceVerification(matrix_backend::BlockingCallContext context,
                                                     uint64_t handleId,
                                                     const QString &userId,
                                                     const QString &deviceId,
                                                     QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall(
          "matrix_start_device_verification", [context, handleId, userId, deviceId]() {
              return ::komai::rust::matrix_start_device_verification(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                userId.toStdString(),
                deviceId.toStdString());
          });
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::unverifyDevice(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &userId,
                                            const QString &deviceId,
                                            QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_unverify_device", [context, handleId, userId, deviceId]() {
            ::komai::rust::matrix_unverify_device(matrix_backend::toRustBlockingContext(context),
                                                  handleId,
                                                  userId.toStdString(),
                                                  deviceId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::blockDevice(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &userId,
                                         const QString &deviceId,
                                         QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_block_device", [context, handleId, userId, deviceId]() {
            ::komai::rust::matrix_block_device(matrix_backend::toRustBlockingContext(context),
                                               handleId,
                                               userId.toStdString(),
                                               deviceId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unblockDevice(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           const QString &userId,
                                           const QString &deviceId,
                                           QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_unblock_device", [context, handleId, userId, deviceId]() {
            ::komai::rust::matrix_unblock_device(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 userId.toStdString(),
                                                 deviceId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixUserVerificationState>
MatrixBackendRuntimeService::fetchUserVerificationState(matrix_backend::BlockingCallContext context,
                                                        uint64_t handleId,
                                                        const QString &userId,
                                                        QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_user_verification_state", [context, handleId, userId]() {
              return ::komai::rust::matrix_fetch_user_verification_state(
                matrix_backend::toRustBlockingContext(context), handleId, userId.toStdString());
          });
        return fromRustUserVerificationState(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<QString>>
MatrixBackendRuntimeService::takePendingVerificationFlowIds(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_take_pending_verification_flow_ids", [context, handleId]() {
              return ::komai::rust::matrix_take_pending_verification_flow_ids(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
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
MatrixBackendRuntimeService::fetchVerificationSession(matrix_backend::BlockingCallContext context,
                                                      uint64_t handleId,
                                                      const QString &flowId,
                                                      QString *errorOut)
{
    try {
        auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_verification_session", [context, handleId, flowId]() {
              return ::komai::rust::matrix_fetch_verification_session(
                matrix_backend::toRustBlockingContext(context), handleId, flowId.toStdString());
          });
        return fromRustVerificationSession(result);
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::clearVerificationSession(matrix_backend::BlockingCallContext context,
                                                      uint64_t handleId,
                                                      const QString &flowId,
                                                      QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_clear_verification_session", [context, handleId, flowId]() {
            ::komai::rust::matrix_clear_verification_session(
              matrix_backend::toRustBlockingContext(context), handleId, flowId.toStdString());
        });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::advanceVerificationSession(matrix_backend::BlockingCallContext context,
                                                        uint64_t handleId,
                                                        const QString &flowId,
                                                        QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall(
          "matrix_advance_verification_session", [context, handleId, flowId]() {
              ::komai::rust::matrix_advance_verification_session(
                matrix_backend::toRustBlockingContext(context), handleId, flowId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::cancelVerificationSession(matrix_backend::BlockingCallContext context,
                                                       uint64_t handleId,
                                                       const QString &flowId,
                                                       bool mismatch,
                                                       QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_cancel_verification_session",
                                [context, handleId, flowId, mismatch]() {
                                    ::komai::rust::matrix_cancel_verification_session(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      flowId.toStdString(),
                                      mismatch);
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
  QString *errorOut)
{
    try {
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_public_room_directory_page",
                                  [context, handleId, searchTerm, limit, since, server]() {
                                      return ::komai::rust::matrix_fetch_public_room_directory_page(
                                        matrix_backend::toRustBlockingContext(context),
                                        handleId,
                                        searchTerm.toStdString(),
                                        limit,
                                        since.toStdString(),
                                        server.toStdString());
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
            rooms.push_back(fromRustRoomSummary(room));
        cachedRoomListSnapshots.insert(handleId, rooms);
        return rooms;
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

bool
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
        invokeRuntimeWorkerCall("matrix_upload_room_avatar",
                                [context, handleId, roomId, filePath, mimeType, width, height]() {
                                    ::komai::rust::matrix_upload_room_avatar(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      filePath.toStdString(),
                                      mimeType.toStdString(),
                                      width,
                                      height);
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
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

bool
MatrixBackendRuntimeService::setActiveRoomTimelineInitialPageSize(uint64_t handleId,
                                                                  uint16_t pageSize,
                                                                  QString *errorOut)
{
    try {
        ::komai::rust::matrix_set_active_room_timeline_initial_page_size(handleId, pageSize);
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QVector<MatrixTimelineItem>>
MatrixBackendRuntimeService::fetchActiveRoomTimeline(matrix_backend::BlockingCallContext context,
                                                     uint64_t handleId,
                                                     QString *errorOut)
{
    try {
        QElapsedTimer totalTimer;
        totalTimer.start();
        const auto result =
          invokeRuntimeWorkerCall("matrix_fetch_active_room_timeline", [context, handleId]() {
              return ::komai::rust::matrix_fetch_active_room_timeline(
                matrix_backend::toRustBlockingContext(context), handleId);
          });
        const auto rustFetchElapsedUs = totalTimer.nsecsElapsed() / 1000;

        QElapsedTimer convertTimer;
        convertTimer.start();
        QVector<MatrixTimelineItem> items;
        items.reserve(static_cast<int>(result.size()));
        for (const auto &item : result)
            items.push_back(fromRustTimelineItem(item));

        const auto convertElapsedUs = convertTimer.nsecsElapsed() / 1000;

        if (roomSwitchPerfEnabled()) {
            QHash<QString, int> itemKindCounts;
            itemKindCounts.reserve(items.size());
            for (const auto &item : items)
                itemKindCounts[item.itemKind] += 1;

            QStringList itemKindSummary;
            itemKindSummary.reserve(itemKindCounts.size());
            for (auto it = itemKindCounts.cbegin(); it != itemKindCounts.cend(); ++it)
                itemKindSummary.push_back(QStringLiteral("%1:%2").arg(it.key()).arg(it.value()));
            itemKindSummary.sort();

            nhlog::ui()->info(
              "[room-switch-perf] phase=cpp.matrix_backend.fetch_active_room_timeline "
              "handle_id={} item_count={} rust_us={} convert_us={} total_us={} item_kinds={}",
              handleId,
              items.size(),
              rustFetchElapsedUs,
              convertElapsedUs,
              totalTimer.nsecsElapsed() / 1000,
              itemKindSummary.join(QStringLiteral(",")).toStdString());
        }

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
MatrixBackendRuntimeService::sendRoomMessage(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &body,
                                             bool useMarkdownFormatting,
                                             const QString &messageKind,
                                             QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_message",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, body, useMarkdownFormatting, messageKind, context]() {
              ::komai::rust::matrix_send_room_message(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                body.toStdString(),
                useMarkdownFormatting,
                messageKind.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::sendRoomMessageLikeEventJson(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &roomId,
  const QString &eventType,
  const QString &contentJson,
  QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_message_like_event_json",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventType, contentJson, context]() {
              ::komai::rust::matrix_send_room_message_like_event_json(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventType.toStdString(),
                contentJson.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

void
MatrixBackendRuntimeService::sendCallInvite(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const std::string &callId,
                                            const std::string &partyId,
                                            const std::string &version,
                                            uint32_t lifetime,
                                            const std::string &invitee,
                                            const std::string &offerSdp,
                                            const std::string &offerType)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_invite",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_invite(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 roomId.toStdString(),
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
MatrixBackendRuntimeService::sendCallCandidates(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId,
                                                const std::string &callId,
                                                const std::string &partyId,
                                                const std::string &version,
                                                const komai::voip::CallIceCandidateList &candidates)
{
    // Copy the candidates into a plain std::vector for lambda capture, then build
    // the rust::Vec inside the blocking call where it will be consumed.
    auto capturedCandidates = candidates;
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_candidates",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=, capturedCandidates = std::move(capturedCandidates)]() {
          ::rust::Vec<::komai::rust::MatrixCallIceCandidate> rustCandidates;
          for (const auto &c : capturedCandidates) {
              ::komai::rust::MatrixCallIceCandidate rc;
              rc.sdp_mid          = ::rust::String(c.sdpMid);
              rc.sdp_m_line_index = c.sdpMLineIndex;
              rc.candidate        = ::rust::String(c.candidate);
              rustCandidates.push_back(std::move(rc));
          }
          ::komai::rust::matrix_send_call_candidates(matrix_backend::toRustBlockingContext(context),
                                                     handleId,
                                                     roomId.toStdString(),
                                                     callId,
                                                     partyId,
                                                     version,
                                                     std::move(rustCandidates));
      });
}

void
MatrixBackendRuntimeService::sendCallAnswer(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const std::string &callId,
                                            const std::string &partyId,
                                            const std::string &version,
                                            const std::string &answerSdp,
                                            const std::string &answerType)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_answer",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_answer(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 roomId.toStdString(),
                                                 callId,
                                                 partyId,
                                                 version,
                                                 answerSdp,
                                                 answerType);
      });
}

void
MatrixBackendRuntimeService::sendCallHangUp(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const std::string &callId,
                                            const std::string &partyId,
                                            const std::string &version,
                                            const std::string &reason)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_hangup",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_hangup(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 roomId.toStdString(),
                                                 callId,
                                                 partyId,
                                                 version,
                                                 reason);
      });
}

void
MatrixBackendRuntimeService::sendCallSelectAnswer(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &roomId,
                                                  const std::string &callId,
                                                  const std::string &partyId,
                                                  const std::string &version,
                                                  const std::string &selectedPartyId)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_select_answer",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_select_answer(
            matrix_backend::toRustBlockingContext(context),
            handleId,
            roomId.toStdString(),
            callId,
            partyId,
            version,
            selectedPartyId);
      });
}

void
MatrixBackendRuntimeService::sendCallReject(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const std::string &callId,
                                            const std::string &partyId,
                                            const std::string &version)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_reject",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_reject(matrix_backend::toRustBlockingContext(context),
                                                 handleId,
                                                 roomId.toStdString(),
                                                 callId,
                                                 partyId,
                                                 version);
      });
}

void
MatrixBackendRuntimeService::sendCallNegotiate(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &roomId,
                                               const std::string &callId,
                                               const std::string &partyId,
                                               uint32_t lifetime,
                                               const std::string &descSdp,
                                               const std::string &descType)
{
    matrix_backend::invokeBlockingCall(
      "matrix_send_call_negotiate",
      matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
      [=]() {
          ::komai::rust::matrix_send_call_negotiate(matrix_backend::toRustBlockingContext(context),
                                                    handleId,
                                                    roomId.toStdString(),
                                                    callId,
                                                    partyId,
                                                    lifetime,
                                                    descSdp,
                                                    descType);
      });
}

bool
MatrixBackendRuntimeService::sendRoomReplyMessage(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &roomId,
                                                  const QString &repliedToEventId,
                                                  const QString &body,
                                                  bool useMarkdownFormatting,
                                                  const QString &messageKind,
                                                  const QString &threadId,
                                                  QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_reply_message",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId,
           roomId,
           repliedToEventId,
           body,
           useMarkdownFormatting,
           messageKind,
           threadId,
           context]() {
              ::komai::rust::matrix_send_room_reply_message(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                repliedToEventId.toStdString(),
                body.toStdString(),
                useMarkdownFormatting,
                messageKind.toStdString(),
                threadId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::sendRoomEditMessage(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 const QString &roomId,
                                                 const QString &targetEventId,
                                                 const QString &body,
                                                 bool useMarkdownFormatting,
                                                 const QString &messageKind,
                                                 QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_edit_message",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, targetEventId, body, useMarkdownFormatting, messageKind, context]() {
              ::komai::rust::matrix_send_room_edit_message(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                targetEventId.toStdString(),
                body.toStdString(),
                useMarkdownFormatting,
                messageKind.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::toggleRoomReaction(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId,
                                                const QString &eventId,
                                                const QString &reactionKey,
                                                QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_toggle_room_reaction",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, reactionKey, context]() {
              ::komai::rust::matrix_toggle_room_reaction(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString(),
                reactionKey.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::redactRoomEvent(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &eventId,
                                             const QString &reason,
                                             QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_redact_room_event",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, reason, context]() {
              ::komai::rust::matrix_redact_room_event(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString(),
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
MatrixBackendRuntimeService::markRoomEventAsRead(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 const QString &roomId,
                                                 const QString &eventId,
                                                 QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_mark_room_event_as_read",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              ::komai::rust::matrix_mark_room_event_as_read(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::reportRoomEvent(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &eventId,
                                             const QString &reason,
                                             int score,
                                             QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_report_room_event",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, reason, score, context]() {
              ::komai::rust::matrix_report_room_event(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString(),
                reason.toStdString(),
                score);
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QStringList>
MatrixBackendRuntimeService::fetchRoomPinnedEventIds(matrix_backend::BlockingCallContext context,
                                                     uint64_t handleId,
                                                     const QString &roomId,
                                                     QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_room_pinned_event_ids",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, context]() {
              return ::komai::rust::matrix_fetch_room_pinned_event_ids(
                matrix_backend::toRustBlockingContext(context), handleId, roomId.toStdString());
          });
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

std::optional<QStringList>
MatrixBackendRuntimeService::fetchRoomFrequentReactions(matrix_backend::BlockingCallContext context,
                                                        uint64_t handleId,
                                                        const QString &roomId,
                                                        int lookbackDays,
                                                        int maxResults,
                                                        uint64_t maxScannedEvents,
                                                        QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_room_frequent_reactions",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, lookbackDays, maxResults, maxScannedEvents, context]() {
              return ::komai::rust::matrix_fetch_room_frequent_reactions(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                lookbackDays,
                maxResults > 0 ? static_cast<uint32_t>(maxResults) : 0,
                maxScannedEvents);
          });
        QStringList frequentReactions;
        frequentReactions.reserve(static_cast<qsizetype>(result.size()));
        for (const auto &reaction : result)
            frequentReactions.push_back(QString::fromStdString(std::string(reaction)));
        return frequentReactions;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::pinRoomEvent(matrix_backend::BlockingCallContext context,
                                          uint64_t handleId,
                                          const QString &roomId,
                                          const QString &eventId,
                                          QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_pin_room_event",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              ::komai::rust::matrix_pin_room_event(matrix_backend::toRustBlockingContext(context),
                                                   handleId,
                                                   roomId.toStdString(),
                                                   eventId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

bool
MatrixBackendRuntimeService::unpinRoomEvent(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const QString &eventId,
                                            QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_unpin_room_event",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              ::komai::rust::matrix_unpin_room_event(matrix_backend::toRustBlockingContext(context),
                                                     handleId,
                                                     roomId.toStdString(),
                                                     eventId.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<MatrixBackendRuntimeService::RawEventDialogData>
MatrixBackendRuntimeService::fetchActiveRoomRawEventDialogData(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &roomId,
  const QString &eventId,
  QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_active_room_raw_event_dialog_data",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              return ::komai::rust::matrix_fetch_active_room_raw_event_dialog_data(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString());
          });
        return RawEventDialogData{
          .prettyJson    = QString::fromStdString(std::string(result.pretty_json)),
          .body          = QString::fromStdString(std::string(result.body)),
          .formattedBody = QString::fromStdString(std::string(result.formatted_body)),
        };
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<MatrixBackendRuntimeService::EventContentForForwarding>
MatrixBackendRuntimeService::fetchActiveRoomEventContentForForwarding(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &roomId,
  const QString &eventId,
  QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_active_room_event_content_for_forwarding",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              return ::komai::rust::matrix_fetch_active_room_event_content_for_forwarding(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString());
          });
        return EventContentForForwarding{
          .eventType   = QString::fromStdString(std::string(result.event_type)),
          .contentJson = QString::fromStdString(std::string(result.content_json)),
        };
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

std::optional<QVector<MatrixReadReceiptEntry>>
MatrixBackendRuntimeService::fetchRoomReadReceipts(matrix_backend::BlockingCallContext context,
                                                   uint64_t handleId,
                                                   const QString &roomId,
                                                   const QString &eventId,
                                                   QString *errorOut)
{
    try {
        const auto result = matrix_backend::invokeBlockingCall(
          "matrix_fetch_room_read_receipts",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId, roomId, eventId, context]() {
              return ::komai::rust::matrix_fetch_room_read_receipts(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                eventId.toStdString());
          });
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
MatrixBackendRuntimeService::sendRoomAttachment(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &roomId,
                                                const QString &filePath,
                                                const QString &filename,
                                                const QString &caption,
                                                const QString &replyEventId,
                                                const QString &threadId,
                                                const QString &mimeType,
                                                QString *errorOut)
{
    try {
        matrix_backend::invokeBlockingCall(
          "matrix_send_room_attachment",
          matrix_backend::BlockingCallThreadPolicy::RequireWorkerThread,
          [handleId,
           roomId,
           filePath,
           filename,
           caption,
           replyEventId,
           threadId,
           mimeType,
           context]() {
              ::komai::rust::matrix_send_room_attachment(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                roomId.toStdString(),
                filePath.toStdString(),
                filename.toStdString(),
                caption.toStdString(),
                replyEventId.toStdString(),
                threadId.toStdString(),
                mimeType.toStdString());
          });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QString>
MatrixBackendRuntimeService::uploadMedia(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &filePath,
                                         const QString &mimeType,
                                         QString *errorOut)
{
    try {
        return matrix::normalizeMxcUri(QString::fromStdString(std::string(
          invokeRuntimeWorkerCall("matrix_upload_media", [context, handleId, filePath, mimeType]() {
              return ::komai::rust::matrix_upload_media(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                filePath.toStdString(),
                mimeType.toStdString());
          }))));
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return std::nullopt;
    }
}

bool
MatrixBackendRuntimeService::sendRoomImage(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           const QString &roomId,
                                           const QString &mxcUri,
                                           const QString &body,
                                           const QString &filename,
                                           const QString &infoJson,
                                           QString *errorOut)
{
    try {
        invokeRuntimeWorkerCall("matrix_send_room_image",
                                [context, handleId, roomId, mxcUri, body, filename, infoJson]() {
                                    ::komai::rust::matrix_send_room_image(
                                      matrix_backend::toRustBlockingContext(context),
                                      handleId,
                                      roomId.toStdString(),
                                      mxcUri.toStdString(),
                                      body.toStdString(),
                                      filename.toStdString(),
                                      infoJson.toStdString());
                                });
        return true;
    } catch (const std::exception &e) {
        if (errorOut)
            *errorOut = QString::fromUtf8(e.what());
        return false;
    }
}

std::optional<QByteArray>
MatrixBackendRuntimeService::fetchActiveRoomTimelineMediaContent(
  matrix_backend::BlockingCallContext context,
  uint64_t handleId,
  const QString &itemId,
  int width,
  int height,
  bool crop,
  QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_active_room_timeline_media_content",
          [context, handleId, &itemId, width, height, crop]() {
              return ::komai::rust::matrix_fetch_active_room_timeline_media_content(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                itemId.toStdString(),
                width,
                height,
                crop);
          });
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
MatrixBackendRuntimeService::fetchMediaContent(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               const QString &mxcUri,
                                               int width,
                                               int height,
                                               bool crop,
                                               QString *errorOut)
{
    try {
        const auto result = invokeRuntimeWorkerCall(
          "matrix_fetch_media_content", [context, handleId, &mxcUri, width, height, crop]() {
              return ::komai::rust::matrix_fetch_media_content(
                matrix_backend::toRustBlockingContext(context),
                handleId,
                mxcUri.toStdString(),
                width,
                height,
                crop);
          });
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
