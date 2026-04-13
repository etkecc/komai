// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVector>
#include <cstdint>
#include <optional>

#include "matrix/MatrixRoomPowerLevels.h"
#include "matrix/backend/MatrixBlockingCall.h"
#include "timeline/Reaction.h"
#include "voip/CallTypes.h"

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

class MatrixBackendRuntimeService
{
public:
    static std::optional<MatrixBackendHandleInfo>
    startRestoredBackend(matrix_backend::BlockingCallContext context,
                         const QString &profileId,
                         QString *errorOut = nullptr);

    static bool logoutBackend(matrix_backend::BlockingCallContext context,
                              uint64_t handleId,
                              QString *errorOut = nullptr);
    static bool stopBackend(uint64_t handleId, QString *errorOut = nullptr);

    /// Start the local media streaming proxy. Returns the listening port.
    static std::optional<uint16_t> startMediaProxy(uint64_t handleId);

    /// Check whether a timeline media item uses encryption (cannot be streamed).
    static bool isTimelineMediaEncrypted(uint64_t handleId, const QString &itemId);

    /// Register a timeline media item for proxy streaming.
    /// Returns the proxy URL on success, or std::nullopt if the item is encrypted,
    /// not found, or the proxy is not running.
    static std::optional<QString> registerTimelineMediaProxyUrl(uint64_t handleId,
                                                                const QString &itemId,
                                                                const QString &extension);

    /// Stop the media proxy. Idempotent.
    static void stopMediaProxy(uint64_t handleId);

    static bool startSync(uint64_t handleId, QString *errorOut = nullptr);

    static MatrixJoinRoomResult joinRoom(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &roomIdOrAlias,
                                         const QVector<QString> &via,
                                         const QString &reason = {});

    static std::optional<QString> knockRoom(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomIdOrAlias,
                                            const QVector<QString> &via,
                                            const QString &reason,
                                            QString *errorOut = nullptr);

    static std::optional<QString> createRoom(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const MatrixCreateRoomRequest &request,
                                             QString *errorOut = nullptr);

    static bool leaveRoom(matrix_backend::BlockingCallContext context,
                          uint64_t handleId,
                          const QString &roomId,
                          const QString &reason = {},
                          QString *errorOut     = nullptr);

    static bool toggleRoomTag(matrix_backend::BlockingCallContext context,
                              uint64_t handleId,
                              const QString &roomId,
                              const QString &tag,
                              bool enabled,
                              QString *errorOut = nullptr);

    static bool setRoomIsDirect(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
                                const QString &roomId,
                                bool isDirect,
                                QString *errorOut = nullptr);

    static bool inviteUser(matrix_backend::BlockingCallContext context,
                           uint64_t handleId,
                           const QString &roomId,
                           const QString &userId,
                           const QString &reason = {},
                           QString *errorOut     = nullptr);

    static bool kickUser(matrix_backend::BlockingCallContext context,
                         uint64_t handleId,
                         const QString &roomId,
                         const QString &userId,
                         const QString &reason = {},
                         QString *errorOut     = nullptr);

    static bool banUser(matrix_backend::BlockingCallContext context,
                        uint64_t handleId,
                        const QString &roomId,
                        const QString &userId,
                        const QString &reason = {},
                        QString *errorOut     = nullptr);

    static bool unbanUser(matrix_backend::BlockingCallContext context,
                          uint64_t handleId,
                          const QString &roomId,
                          const QString &userId,
                          const QString &reason = {},
                          QString *errorOut     = nullptr);

    static std::optional<MatrixOwnProfile>
    fetchOwnProfile(matrix_backend::BlockingCallContext context,
                    uint64_t handleId,
                    QString *errorOut = nullptr);

    static std::optional<MatrixOwnPresence>
    fetchOwnPresence(matrix_backend::BlockingCallContext context,
                     uint64_t handleId,
                     QString *errorOut = nullptr);

    static std::optional<MatrixRecoveryStatus>
    fetchRecoveryStatus(matrix_backend::BlockingCallContext context,
                        uint64_t handleId,
                        QString *errorOut = nullptr);

    static std::optional<MatrixSetupRecoveryResult>
    setupRecovery(matrix_backend::BlockingCallContext context,
                  uint64_t handleId,
                  bool useSSSS,
                  const QString &passphrase,
                  bool encryptionBackupOnlineEnabled,
                  QString *errorOut = nullptr);

    static bool recoverEncryptionSecrets(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &keyOrPassphrase,
                                         QString *errorOut = nullptr);

    static std::optional<MatrixResetEncryptionIdentityResult>
    startResetEncryptionIdentity(matrix_backend::BlockingCallContext context,
                                 uint64_t handleId,
                                 QString *errorOut = nullptr);

    static bool
    continueResetEncryptionIdentityWithPassword(matrix_backend::BlockingCallContext context,
                                                uint64_t handleId,
                                                const QString &password,
                                                QString *errorOut = nullptr);

    static bool
    continueResetEncryptionIdentityAfterApproval(matrix_backend::BlockingCallContext context,
                                                 uint64_t handleId,
                                                 QString *errorOut = nullptr);

    static bool cancelResetEncryptionIdentity(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              QString *errorOut = nullptr);

    static std::optional<MatrixDeviceSignOutResult>
    startSignOutDevice(matrix_backend::BlockingCallContext context,
                       uint64_t handleId,
                       const QString &deviceId,
                       QString *errorOut = nullptr);

    static bool continueSignOutDeviceWithPassword(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &password,
                                                  QString *errorOut = nullptr);

    static bool renameDevice(matrix_backend::BlockingCallContext context,
                             uint64_t handleId,
                             const QString &deviceId,
                             const QString &displayName,
                             QString *errorOut = nullptr);

    static std::optional<MatrixVerificationSession>
    startSelfVerification(matrix_backend::BlockingCallContext context,
                          uint64_t handleId,
                          QString *errorOut = nullptr);

    static std::optional<MatrixVerificationSession>
    startUserVerification(matrix_backend::BlockingCallContext context,
                          uint64_t handleId,
                          const QString &userId,
                          QString *errorOut = nullptr);

    static std::optional<MatrixVerificationSession>
    startDeviceVerification(matrix_backend::BlockingCallContext context,
                            uint64_t handleId,
                            const QString &userId,
                            const QString &deviceId,
                            QString *errorOut = nullptr);

    static bool unverifyDevice(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &userId,
                               const QString &deviceId,
                               QString *errorOut = nullptr);

    static bool blockDevice(matrix_backend::BlockingCallContext context,
                            uint64_t handleId,
                            const QString &userId,
                            const QString &deviceId,
                            QString *errorOut = nullptr);

    static bool unblockDevice(matrix_backend::BlockingCallContext context,
                              uint64_t handleId,
                              const QString &userId,
                              const QString &deviceId,
                              QString *errorOut = nullptr);

    static std::optional<MatrixUserVerificationState>
    fetchUserVerificationState(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &userId,
                               QString *errorOut = nullptr);

    static std::optional<QVector<QString>>
    takePendingVerificationFlowIds(matrix_backend::BlockingCallContext context,
                                   uint64_t handleId,
                                   QString *errorOut = nullptr);

    static std::optional<MatrixVerificationSession>
    fetchVerificationSession(matrix_backend::BlockingCallContext context,
                             uint64_t handleId,
                             const QString &flowId,
                             QString *errorOut = nullptr);

    static bool clearVerificationSession(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &flowId,
                                         QString *errorOut = nullptr);

    static bool advanceVerificationSession(matrix_backend::BlockingCallContext context,
                                           uint64_t handleId,
                                           const QString &flowId,
                                           QString *errorOut = nullptr);

    static bool cancelVerificationSession(matrix_backend::BlockingCallContext context,
                                          uint64_t handleId,
                                          const QString &flowId,
                                          bool mismatch,
                                          QString *errorOut = nullptr);

    static std::optional<MatrixUserProfile>
    fetchUserProfile(matrix_backend::BlockingCallContext context,
                     uint64_t handleId,
                     const QString &userId,
                     QString *errorOut = nullptr);

    static std::optional<MatrixUserProfile>
    fetchRoomMemberProfile(matrix_backend::BlockingCallContext context,
                           uint64_t handleId,
                           const QString &roomId,
                           const QString &userId,
                           QString *errorOut = nullptr);

    static std::optional<QVector<MatrixDirectoryUser>>
    searchUsers(matrix_backend::BlockingCallContext context,
                uint64_t handleId,
                const QString &searchTerm,
                uint64_t limit,
                QString *errorOut = nullptr);

    static std::optional<MatrixPublicRoomDirectoryPage>
    fetchPublicRoomDirectoryPage(matrix_backend::BlockingCallContext context,
                                 uint64_t handleId,
                                 const QString &searchTerm,
                                 uint64_t limit,
                                 const QString &since,
                                 const QString &server,
                                 const QString &roomTypeFilter,
                                 QString *errorOut = nullptr);

    static bool setOwnDisplayName(matrix_backend::BlockingCallContext context,
                                  uint64_t handleId,
                                  const QString &displayName,
                                  QString *errorOut = nullptr);

    static bool setOwnPresence(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &presenceState,
                               const QString &statusMessage,
                               QString *errorOut = nullptr);

    static bool setOwnRoomDisplayName(matrix_backend::BlockingCallContext context,
                                      uint64_t handleId,
                                      const QString &roomId,
                                      const QString &displayName,
                                      QString *errorOut = nullptr);

    static bool uploadOwnAvatar(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
                                const QString &filePath,
                                const QString &mimeType,
                                QString *errorOut = nullptr);

    static bool removeOwnAvatar(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
                                QString *errorOut = nullptr);

    static bool uploadOwnRoomAvatar(matrix_backend::BlockingCallContext context,
                                    uint64_t handleId,
                                    const QString &roomId,
                                    const QString &filePath,
                                    const QString &mimeType,
                                    QString *errorOut = nullptr);

    static bool removeOwnRoomAvatar(matrix_backend::BlockingCallContext context,
                                    uint64_t handleId,
                                    const QString &roomId,
                                    QString *errorOut = nullptr);

    static bool ignoreUser(matrix_backend::BlockingCallContext context,
                           uint64_t handleId,
                           const QString &userId,
                           QString *errorOut = nullptr);

    static bool unignoreUser(matrix_backend::BlockingCallContext context,
                             uint64_t handleId,
                             const QString &userId,
                             QString *errorOut = nullptr);

    static std::optional<QVector<MatrixRoomSummary>>
    fetchRoomList(matrix_backend::BlockingCallContext context,
                  uint64_t handleId,
                  QString *errorOut = nullptr);
    static std::optional<QVector<MatrixNotificationItem>>
    fetchNotificationItems(matrix_backend::BlockingCallContext context,
                           uint64_t handleId,
                           const QVector<MatrixNotificationRequest> &requests,
                           QString *errorOut = nullptr);
    static std::optional<bool>
    fetchAccountNotificationsEnabled(matrix_backend::BlockingCallContext context,
                                     uint64_t handleId,
                                     QString *errorOut = nullptr);
    static std::optional<MatrixTurnServerInfo>
    fetchTurnServerInfo(matrix_backend::BlockingCallContext context,
                        uint64_t handleId,
                        QString *errorOut = nullptr);
    static bool setAccountNotificationsEnabled(matrix_backend::BlockingCallContext context,
                                               uint64_t handleId,
                                               bool enabled,
                                               QString *errorOut = nullptr);
    static std::optional<QVector<MatrixImagePack>>
    fetchImagePacks(matrix_backend::BlockingCallContext context,
                    uint64_t handleId,
                    const QString &roomId,
                    QString *errorOut = nullptr);
    static bool saveImagePack(matrix_backend::BlockingCallContext context,
                              uint64_t handleId,
                              const QString &roomId,
                              const QString &stateKey,
                              const QString &previousStateKey,
                              bool hasPreviousStateKey,
                              const MatrixImagePack &pack,
                              QString *errorOut = nullptr);
    static bool removeImagePack(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
                                const QString &roomId,
                                const QString &stateKey,
                                QString *errorOut = nullptr);
    static bool setImagePackGloballyEnabled(matrix_backend::BlockingCallContext context,
                                            uint64_t handleId,
                                            const QString &roomId,
                                            const QString &stateKey,
                                            bool enabled,
                                            QString *errorOut = nullptr);
    static void cacheRoomListSnapshot(uint64_t handleId, QVector<MatrixRoomSummary> rooms);
    static void clearCachedRoomListSnapshot(uint64_t handleId);

    static std::optional<MatrixRoomSettings>
    fetchRoomSettings(matrix_backend::BlockingCallContext context,
                      uint64_t handleId,
                      const QString &roomId,
                      QString *errorOut = nullptr);

    static std::optional<MatrixRoomAliases>
    fetchRoomAliases(matrix_backend::BlockingCallContext context,
                     uint64_t handleId,
                     const QString &roomId,
                     QString *errorOut = nullptr);

    static bool applyRoomAliases(matrix_backend::BlockingCallContext context,
                                 uint64_t handleId,
                                 const QString &roomId,
                                 const MatrixRoomAliases &aliases,
                                 QString *errorOut = nullptr);

    static std::optional<QVector<MatrixRoomMember>>
    fetchRoomMembers(matrix_backend::BlockingCallContext context,
                     uint64_t handleId,
                     const QString &roomId,
                     QString *errorOut = nullptr);

    static std::optional<MatrixRoomPowerLevels>
    fetchRoomPowerLevels(matrix_backend::BlockingCallContext context,
                         uint64_t handleId,
                         const QString &roomId,
                         QString *errorOut = nullptr);

    static bool applyRoomPowerLevels(matrix_backend::BlockingCallContext context,
                                     uint64_t handleId,
                                     const QString &roomId,
                                     const MatrixRoomPowerLevels &powerLevels,
                                     QString *errorOut = nullptr);

    static std::optional<QVector<MatrixChildSpaceEntry>>
    fetchRoomChildSpaces(matrix_backend::BlockingCallContext context,
                         uint64_t handleId,
                         const QString &roomId,
                         QString *errorOut = nullptr);

    static std::optional<MatrixRoomRedactionPermissions>
    fetchRoomRedactionPermissions(matrix_backend::BlockingCallContext context,
                                  uint64_t handleId,
                                  const QString &roomId,
                                  QString *errorOut = nullptr);

    static bool setRoomNotificationMode(matrix_backend::BlockingCallContext context,
                                        uint64_t handleId,
                                        const QString &roomId,
                                        int mode,
                                        QString *errorOut = nullptr);

    static bool setRoomName(matrix_backend::BlockingCallContext context,
                            uint64_t handleId,
                            const QString &roomId,
                            const QString &name,
                            QString *errorOut = nullptr);

    static bool setRoomTopic(matrix_backend::BlockingCallContext context,
                             uint64_t handleId,
                             const QString &roomId,
                             const QString &topic,
                             QString *errorOut = nullptr);

    static bool uploadRoomAvatar(matrix_backend::BlockingCallContext context,
                                 uint64_t handleId,
                                 const QString &roomId,
                                 const QString &filePath,
                                 const QString &mimeType,
                                 int width,
                                 int height,
                                 QString *errorOut = nullptr);

    static bool removeRoomAvatar(matrix_backend::BlockingCallContext context,
                                 uint64_t handleId,
                                 const QString &roomId,
                                 QString *errorOut = nullptr);

    static bool enableRoomEncryption(matrix_backend::BlockingCallContext context,
                                     uint64_t handleId,
                                     const QString &roomId,
                                     QString *errorOut = nullptr);

    static bool setRoomHistoryVisibility(matrix_backend::BlockingCallContext context,
                                         uint64_t handleId,
                                         const QString &roomId,
                                         const QString &historyVisibility,
                                         QString *errorOut = nullptr);

    static bool setRoomAccessRules(matrix_backend::BlockingCallContext context,
                                   uint64_t handleId,
                                   const QString &roomId,
                                   const QString &joinRule,
                                   bool guestAccess,
                                   const QVector<QString> &allowedRoomIds,
                                   QString *errorOut = nullptr);

    static bool
    selectActiveRoomTimeline(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static bool
    stopRoomTimeline(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static std::optional<QVector<MatrixTimelineItem>>
    fetchRoomTimelineSnapshot(matrix_backend::BlockingCallContext context,
                              uint64_t handleId,
                              const QString &roomId,
                              QString *errorOut = nullptr);

    static bool setActiveRoomTimelineInitialPageSize(uint64_t handleId,
                                                     uint16_t pageSize,
                                                     QString *errorOut = nullptr);

    static std::optional<QVector<MatrixTimelineItem>>
    fetchActiveRoomTimeline(matrix_backend::BlockingCallContext context,
                            uint64_t handleId,
                            QString *errorOut = nullptr);

    static std::optional<QVector<MatrixTimelineItem>>
    fetchRoomTimeline(matrix_backend::BlockingCallContext context,
                      uint64_t handleId,
                      const QString &roomId,
                      uint16_t limit,
                      QString *errorOut = nullptr);

    static bool paginateActiveRoomTimelineBackwards(uint64_t handleId,
                                                    uint16_t pageSize,
                                                    QString *errorOut = nullptr);

    static bool sendTypingNotice(matrix_backend::BlockingCallContext context,
                                 uint64_t handleId,
                                 const QString &roomId,
                                 bool typing,
                                 QString *errorOut = nullptr);
    static bool sendRoomMessage(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
                                const QString &roomId,
                                const QString &body,
                                bool useMarkdownFormatting,
                                const QString &messageKind,
                                QString *errorOut = nullptr);
    static bool sendRoomMessageLikeEventJson(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &eventType,
                                             const QString &contentJson,
                                             QString *errorOut = nullptr);

    static void sendCallInvite(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &roomId,
                               const std::string &callId,
                               const std::string &partyId,
                               const std::string &version,
                               uint32_t lifetime,
                               const std::string &invitee,
                               const std::string &offerSdp,
                               const std::string &offerType);

    static void sendCallCandidates(matrix_backend::BlockingCallContext context,
                                   uint64_t handleId,
                                   const QString &roomId,
                                   const std::string &callId,
                                   const std::string &partyId,
                                   const std::string &version,
                                   const komai::voip::CallIceCandidateList &candidates);

    static void sendCallAnswer(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &roomId,
                               const std::string &callId,
                               const std::string &partyId,
                               const std::string &version,
                               const std::string &answerSdp,
                               const std::string &answerType);

    static void sendCallHangUp(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &roomId,
                               const std::string &callId,
                               const std::string &partyId,
                               const std::string &version,
                               const std::string &reason);

    static void sendCallSelectAnswer(matrix_backend::BlockingCallContext context,
                                     uint64_t handleId,
                                     const QString &roomId,
                                     const std::string &callId,
                                     const std::string &partyId,
                                     const std::string &version,
                                     const std::string &selectedPartyId);

    static void sendCallReject(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &roomId,
                               const std::string &callId,
                               const std::string &partyId,
                               const std::string &version);

    static void sendCallNegotiate(matrix_backend::BlockingCallContext context,
                                  uint64_t handleId,
                                  const QString &roomId,
                                  const std::string &callId,
                                  const std::string &partyId,
                                  uint32_t lifetime,
                                  const std::string &descSdp,
                                  const std::string &descType);

    static bool sendRoomReplyMessage(matrix_backend::BlockingCallContext context,
                                     uint64_t handleId,
                                     const QString &roomId,
                                     const QString &repliedToEventId,
                                     const QString &body,
                                     bool useMarkdownFormatting,
                                     const QString &messageKind,
                                     const QString &threadId = QString(),
                                     QString *errorOut       = nullptr);

    static bool sendRoomEditMessage(matrix_backend::BlockingCallContext context,
                                    uint64_t handleId,
                                    const QString &roomId,
                                    const QString &targetEventId,
                                    const QString &body,
                                    bool useMarkdownFormatting,
                                    const QString &messageKind,
                                    QString *errorOut = nullptr);

    static bool toggleRoomReaction(matrix_backend::BlockingCallContext context,
                                   uint64_t handleId,
                                   const QString &roomId,
                                   const QString &eventId,
                                   const QString &reactionKey,
                                   QString *errorOut = nullptr);

    static bool redactRoomEvent(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
                                const QString &roomId,
                                const QString &eventId,
                                const QString &reason,
                                QString *errorOut = nullptr);

    static bool markRoomEventAsRead(matrix_backend::BlockingCallContext context,
                                    uint64_t handleId,
                                    const QString &roomId,
                                    const QString &eventId,
                                    QString *errorOut = nullptr);

    static bool reportRoomEvent(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
                                const QString &roomId,
                                const QString &eventId,
                                const QString &reason,
                                int score,
                                QString *errorOut = nullptr);

    static std::optional<QStringList>
    fetchRoomPinnedEventIds(matrix_backend::BlockingCallContext context,
                            uint64_t handleId,
                            const QString &roomId,
                            QString *errorOut = nullptr);

    static std::optional<QStringList>
    fetchRoomFrequentReactions(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &roomId,
                               int lookbackDays,
                               int maxResults,
                               uint64_t maxScannedEvents,
                               QString *errorOut = nullptr);

    static bool pinRoomEvent(matrix_backend::BlockingCallContext context,
                             uint64_t handleId,
                             const QString &roomId,
                             const QString &eventId,
                             QString *errorOut = nullptr);

    static bool unpinRoomEvent(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &roomId,
                               const QString &eventId,
                               QString *errorOut = nullptr);

    struct RawEventDialogData
    {
        QString prettyJson;
        QString body;
        QString formattedBody;
    };

    static std::optional<RawEventDialogData>
    fetchActiveRoomRawEventDialogData(matrix_backend::BlockingCallContext context,
                                      uint64_t handleId,
                                      const QString &roomId,
                                      const QString &eventId,
                                      QString *errorOut = nullptr);

    struct EventContentForForwarding
    {
        QString eventType;
        QString contentJson;
    };

    static std::optional<EventContentForForwarding>
    fetchActiveRoomEventContentForForwarding(matrix_backend::BlockingCallContext context,
                                             uint64_t handleId,
                                             const QString &roomId,
                                             const QString &eventId,
                                             QString *errorOut = nullptr);

    static std::optional<QVector<MatrixReadReceiptEntry>>
    fetchRoomReadReceipts(matrix_backend::BlockingCallContext context,
                          uint64_t handleId,
                          const QString &roomId,
                          const QString &eventId,
                          QString *errorOut = nullptr);

    static bool sendRoomAttachment(matrix_backend::BlockingCallContext context,
                                   uint64_t handleId,
                                   const QString &roomId,
                                   const QString &filePath,
                                   const QString &filename,
                                   const QString &caption,
                                   const QString &replyEventId,
                                   const QString &threadId,
                                   const QString &mimeType,
                                   uint64_t durationMs          = 0,
                                   bool isVoice                 = false,
                                   const QList<float> &waveform = {},
                                   QString *errorOut            = nullptr);

    static std::optional<QString> uploadMedia(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &filePath,
                                              const QString &mimeType,
                                              QString *errorOut = nullptr);

    static bool sendRoomImage(matrix_backend::BlockingCallContext context,
                              uint64_t handleId,
                              const QString &roomId,
                              const QString &mxcUri,
                              const QString &body,
                              const QString &filename,
                              const QString &infoJson,
                              QString *errorOut = nullptr);

    static std::optional<QByteArray>
    fetchActiveRoomTimelineMediaContent(matrix_backend::BlockingCallContext context,
                                        uint64_t handleId,
                                        const QString &itemId,
                                        int width,
                                        int height,
                                        bool crop,
                                        QString *errorOut = nullptr);

    static std::optional<QByteArray> fetchMediaContent(matrix_backend::BlockingCallContext context,
                                                       uint64_t handleId,
                                                       const QString &mxcUri,
                                                       int width,
                                                       int height,
                                                       bool crop,
                                                       QString *errorOut = nullptr);
};

} // namespace komai
