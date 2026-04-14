// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <cstdint>

#include "matrix/MatrixRoomPowerLevels.h"

namespace komai {

struct MatrixBackendHandleInfo
{
    uint64_t handleId = 0;
    bool hasSession   = false;
    QString authType;
    QString homeserverUrl;
    QString userId;
    QString deviceId;
};

struct MatrixOwnProfile
{
    QString displayName;
    QString avatarUrl;
};

struct MatrixOwnPresence
{
    QString state;
    QString statusMessage;
};

struct MatrixTurnServerInfo
{
    QString username;
    QString password;
    QVector<QString> uris;
    uint64_t ttlSeconds = 0;
};

struct MatrixRecoveryStatus
{
    QString state;
    bool hasDevicesToVerifyAgainst = false;
    bool ownDeviceIsVerified       = false;
    bool hasUnverifiedOwnDevices   = false;
};

struct MatrixSetupRecoveryResult
{
    QString recoveryKey;
};

struct MatrixResetEncryptionIdentityResult
{
    bool completed = false;
    QString authType;
    QString approvalUrl;
};

struct MatrixDeviceSignOutResult
{
    bool completed = false;
    QString authType;
    QString approvalUrl;
};

struct MatrixVerificationSession
{
    QString flowId;
    QString userId;
    QString deviceId;
    QString state;
    QString error;
    bool sender                    = false;
    bool isSelfVerification        = false;
    bool isMultiDeviceVerification = false;
    QVector<int> sasNumbers;
};

struct MatrixUserDevice
{
    QString deviceId;
    QString displayName;
    QString verificationState;
    QString lastIp;
    uint64_t lastTs = 0;
};

struct MatrixUserVerificationState
{
    bool hasMasterKey = false;
    QString userTrust;
    QVector<MatrixUserDevice> devices;
};

struct MatrixUserProfile
{
    QString displayName;
    QString avatarUrl;
};

struct MatrixDirectoryUser
{
    QString displayName;
    QString userId;
    QString avatarUrl;
};

struct MatrixPublicRoomDirectoryEntry
{
    QString roomId;
    QString roomServerName;
    QString displayName;
    QString avatarUrl;
    QString topic;
    QString canonicalAlias;
    uint64_t memberCount = 0;
    bool isWorldReadable = false;
    bool isSpace         = false;
};

struct MatrixPublicRoomDirectoryPage
{
    QVector<MatrixPublicRoomDirectoryEntry> rooms;
    QString nextBatch;
    int totalRoomCountEstimate = -1;
};

struct MatrixRoomSummary
{
    QString roomId;
    QString latestEventId;
    QString displayName;
    QString avatarUrl;
    QString topic;
    QString roomAlias;
    QString lastMessage;
    QString lastMessageKind;
    QString lastMessageSenderId;
    QString lastMessageSenderDisplayName;
    QVector<QString> tags;
    QVector<QString> parentSpaceRoomIds;
    QString directChatOtherUserId;
    QString inviterUserId;
    QString inviterDisplayName;
    QString inviterAvatarUrl;
    QString inviteReason;
    bool isInvite              = false;
    bool isSpace               = false;
    bool isDirect              = false;
    bool isBotRoom             = false;
    bool isEncrypted           = false;
    bool isPublic              = false;
    uint64_t memberCount       = 0;
    uint64_t unreadMessages    = 0;
    uint64_t notificationCount = 0;
    uint64_t highlightCount    = 0;
    uint64_t timestamp         = 0;
};

struct MatrixNotificationRequest
{
    QString roomId;
    QString eventId;
};

struct MatrixNotificationItem
{
    QString roomId;
    QString eventId;
    QString replacementEventId;
    QString roomName;
    QString avatarUrl;
    QString senderDisplayName;
    QString notificationKind;
    QString plainBody;
    QString formattedBody;
    QString mediaMxcUrl;
    bool isReply         = false;
    bool isEmote         = false;
    bool isEncrypted     = false;
    bool containsSpoiler = false;
    bool hasInlineImage  = false;
    bool playSound       = false;
};

struct MatrixImagePackImage
{
    QString shortcode;
    QString body;
    QString url;
    bool isEmote   = false;
    bool isSticker = false;
};

struct MatrixImagePack
{
    QString sourceRoomId;
    QString stateKey;
    QString displayName;
    QString avatarUrl;
    QString attribution;
    bool isEmotePack       = true;
    bool isStickerPack     = true;
    bool fromSpace         = false;
    bool isGloballyEnabled = false;
    QVector<MatrixImagePackImage> images;
};

struct MatrixRoomSettings
{
    QString roomId;
    QString roomName;
    QString roomTopic;
    QString roomAvatarUrl;
    QString roomVersion;
    uint64_t memberCount = 0;
    int notifications    = 2;
    QString joinRule;
    QString historyVisibility;
    QVector<QString> allowedRoomIds;
    QVector<QString> parentSpaceRoomIds;
    bool guestAccess                = false;
    bool isEncrypted                = false;
    bool canChangeName              = false;
    bool canChangeTopic             = false;
    bool canChangeAvatar            = false;
    bool canChangeJoinRules         = false;
    bool canChangeHistoryVisibility = false;
};

struct MatrixRoomAliases
{
    QString canonicalAlias;
    QVector<QString> altAliases;
    QVector<QString> publishedAliases;
};

struct MatrixRoomMember
{
    QString userId;
    QString displayName;
    QString avatarUrl;
    qlonglong powerLevel = 0;
    bool isInvited       = false;
};

struct MatrixChildSpaceEntry
{
    QString roomId;
    QString displayName;
    QString avatarUrl;
    MatrixRoomPowerLevels powerLevels;
};

struct MatrixRoomRedactionPermissions
{
    bool canRedactOwn   = false;
    bool canRedactOther = false;
};

struct MatrixReadReceiptEntry
{
    QString userId;
    QString displayName;
    QString avatarUrl;
    uint64_t timestamp = 0;
};

struct PowerLevelChange
{
    QString userId;
    int64_t oldLevel                                = 0;
    int64_t newLevel                                = 0;
    bool operator==(const PowerLevelChange &) const = default;
};

struct MatrixTimelineItem
{
    QString itemId;
    QString eventId;
    QString deliveryState;
    QString threadId;
    bool isThreadRoot = false;
    QString senderId;
    QString senderDisplayName;
    QString senderAvatarUrl;
    QString body;
    QString formattedBody;
    QString replyEventId;
    QString replySenderId;
    QString replySenderDisplayName;
    QString replyItemKind;
    QString replyMatrixEventType;
    QString replyBody;
    QString replyFormattedBody;
    QString replyMediaUrl;
    QString replyThumbnailUrl;
    QString replyFileName;
    QString replyMimeType;
    uint64_t replyMediaWidth      = 0;
    uint64_t replyMediaHeight     = 0;
    uint64_t replyMediaDurationMs = 0;
    uint64_t replyMediaSizeBytes  = 0;
    QString replyBlurhash;
    QVariantList reactions;
    QString reactionsSummary;
    QStringList specialEffectNames;
    QString itemKind;
    QString membershipChangeKind;
    QString matrixEventType;
    bool isEdited = false;
    QString mediaUrl;
    QString thumbnailUrl;
    QString fileName;
    QString mimeType;
    uint64_t mediaWidth      = 0;
    uint64_t mediaHeight     = 0;
    uint64_t mediaDurationMs = 0;
    uint64_t mediaSizeBytes  = 0;
    QString blurhash;
    bool mediaIsEncrypted     = false;
    bool thumbnailIsEncrypted = false;
    bool isVoiceMessage       = false;
    QList<float> waveform;
    uint64_t timestamp = 0;
    bool isOwn         = false;
    // Structured state event parameters (populated by Rust, translated by C++).
    QString stateEventTargetUser;
    QString stateEventTargetUserId;
    QString stateEventDetail;
    QString stateEventReason;
    bool stateEventHasSender = false;
    QList<PowerLevelChange> powerLevelChanges;
    // Pre-computed derived fields (populated by MatrixTimelineModel, not the Rust bridge).
    int cachedType             = 0;
    int cachedEmojiOnlyCount   = 0;
    int cachedDay              = 0;
    int cachedStatus           = 0;
    bool cachedIsStateEvent    = false;
    bool cachedIsEncrypted     = false;
    bool cachedIsEditable      = false;
    bool cachedIsHiddenEvent   = false;
    double cachedProportionalH = 0.0;
    QString cachedFormattedBody;
    QString cachedFormattedStateEvent;
    QString cachedStateEventIcon;
    QString cachedStateEventIconColorCategory;
    QString cachedFilesize;
    QString cachedFilename;
    QString cachedFileTypeIcon;

    bool operator==(const MatrixTimelineItem &) const = default;
};

struct MatrixJoinRoomResult
{
    bool ok = false;
    QString roomId;
    QString error;
    QString matrixErrcode;
};

enum class MatrixCreateRoomPreset
{
    PrivateChat,
    PublicChat,
    TrustedPrivateChat,
};

struct MatrixCreateRoomRequest
{
    QString name;
    QString topic;
    QString roomAliasLocalpart;
    QVector<QString> inviteUserIds;
    MatrixCreateRoomPreset preset = MatrixCreateRoomPreset::PrivateChat;
    bool isDirect                 = false;
    bool isEncrypted              = false;
    bool isSpace                  = false;
    bool isPublic                 = false;
};

} // namespace komai
