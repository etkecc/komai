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

#include "timeline/Reaction.h"

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
    QString lastMessage;
    QString lastMessageKind;
    QVector<QString> tags;
    QVector<QString> parentSpaceRoomIds;
    QString directChatOtherUserId;
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

struct MatrixRoomMember
{
    QString userId;
    QString displayName;
    QString avatarUrl;
    qlonglong powerLevel = 0;
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

struct MatrixTimelineItem
{
    QString itemId;
    QString eventId;
    QString deliveryState;
    QString threadId;
    QString senderId;
    QString senderDisplayName;
    QString senderAvatarUrl;
    QString body;
    QString formattedBody;
    QString replyEventId;
    QString replySenderId;
    QString replySenderDisplayName;
    QString replyBody;
    QString replyFormattedBody;
    QVariantList reactions;
    QString reactionsSummary;
    QStringList specialEffectNames;
    QString itemKind;
    bool isEdited = false;
    QString mediaUrl;
    QString thumbnailUrl;
    QString fileName;
    QString mimeType;
    uint64_t mediaWidth       = 0;
    uint64_t mediaHeight      = 0;
    uint64_t mediaDurationMs  = 0;
    uint64_t mediaSizeBytes   = 0;
    bool mediaIsEncrypted     = false;
    bool thumbnailIsEncrypted = false;
    uint64_t timestamp        = 0;
    bool isOwn                = false;
    // Pre-computed derived fields (populated by MatrixTimelineModel, not the Rust bridge).
    int cachedType             = 0;
    int cachedEmojiOnlyCount   = 0;
    int cachedDay              = 0;
    int cachedStatus           = 0;
    bool cachedIsStateEvent    = false;
    bool cachedIsEncrypted     = false;
    bool cachedIsEditable      = false;
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
    startRestoredBackend(const QString &profileId, QString *errorOut = nullptr);

    static bool logoutBackend(uint64_t handleId, QString *errorOut = nullptr);
    static bool stopBackend(uint64_t handleId, QString *errorOut = nullptr);
    static bool startSync(uint64_t handleId, QString *errorOut = nullptr);

    static MatrixJoinRoomResult joinRoom(uint64_t handleId,
                                         const QString &roomIdOrAlias,
                                         const QVector<QString> &via,
                                         const QString &reason = {});

    static std::optional<QString> knockRoom(uint64_t handleId,
                                            const QString &roomIdOrAlias,
                                            const QVector<QString> &via,
                                            const QString &reason,
                                            QString *errorOut = nullptr);

    static std::optional<QString> createRoom(uint64_t handleId,
                                             const MatrixCreateRoomRequest &request,
                                             QString *errorOut = nullptr);

    static bool leaveRoom(uint64_t handleId,
                          const QString &roomId,
                          const QString &reason = {},
                          QString *errorOut     = nullptr);

    static bool toggleRoomTag(uint64_t handleId,
                              const QString &roomId,
                              const QString &tag,
                              bool enabled,
                              QString *errorOut = nullptr);

    static bool inviteUser(uint64_t handleId,
                           const QString &roomId,
                           const QString &userId,
                           const QString &reason = {},
                           QString *errorOut     = nullptr);

    static bool kickUser(uint64_t handleId,
                         const QString &roomId,
                         const QString &userId,
                         const QString &reason = {},
                         QString *errorOut     = nullptr);

    static bool banUser(uint64_t handleId,
                        const QString &roomId,
                        const QString &userId,
                        const QString &reason = {},
                        QString *errorOut     = nullptr);

    static bool unbanUser(uint64_t handleId,
                          const QString &roomId,
                          const QString &userId,
                          const QString &reason = {},
                          QString *errorOut     = nullptr);

    static std::optional<MatrixOwnProfile>
    fetchOwnProfile(uint64_t handleId, QString *errorOut = nullptr);

    static std::optional<MatrixRecoveryStatus>
    fetchRecoveryStatus(uint64_t handleId, QString *errorOut = nullptr);

    static std::optional<MatrixSetupRecoveryResult>
    setupRecovery(uint64_t handleId,
                  bool useSSSS,
                  const QString &passphrase,
                  bool encryptionBackupOnlineEnabled,
                  QString *errorOut = nullptr);

    static bool recoverEncryptionSecrets(uint64_t handleId,
                                         const QString &keyOrPassphrase,
                                         QString *errorOut = nullptr);

    static std::optional<MatrixResetEncryptionIdentityResult>
    startResetEncryptionIdentity(uint64_t handleId, QString *errorOut = nullptr);

    static bool continueResetEncryptionIdentityWithPassword(uint64_t handleId,
                                                            const QString &password,
                                                            QString *errorOut = nullptr);

    static bool
    continueResetEncryptionIdentityAfterApproval(uint64_t handleId, QString *errorOut = nullptr);

    static bool cancelResetEncryptionIdentity(uint64_t handleId, QString *errorOut = nullptr);

    static std::optional<MatrixDeviceSignOutResult>
    startSignOutDevice(uint64_t handleId, const QString &deviceId, QString *errorOut = nullptr);

    static bool continueSignOutDeviceWithPassword(uint64_t handleId,
                                                  const QString &password,
                                                  QString *errorOut = nullptr);

    static bool renameDevice(uint64_t handleId,
                             const QString &deviceId,
                             const QString &displayName,
                             QString *errorOut = nullptr);

    static std::optional<MatrixVerificationSession>
    startSelfVerification(uint64_t handleId, QString *errorOut = nullptr);

    static std::optional<MatrixVerificationSession>
    startUserVerification(uint64_t handleId, const QString &userId, QString *errorOut = nullptr);

    static std::optional<MatrixVerificationSession>
    startDeviceVerification(uint64_t handleId,
                            const QString &userId,
                            const QString &deviceId,
                            QString *errorOut = nullptr);

    static bool unverifyDevice(uint64_t handleId,
                               const QString &userId,
                               const QString &deviceId,
                               QString *errorOut = nullptr);

    static bool blockDevice(uint64_t handleId,
                            const QString &userId,
                            const QString &deviceId,
                            QString *errorOut = nullptr);

    static bool unblockDevice(uint64_t handleId,
                              const QString &userId,
                              const QString &deviceId,
                              QString *errorOut = nullptr);

    static std::optional<MatrixUserVerificationState>
    fetchUserVerificationState(uint64_t handleId,
                               const QString &userId,
                               QString *errorOut = nullptr);

    static std::optional<QVector<QString>>
    takePendingVerificationFlowIds(uint64_t handleId, QString *errorOut = nullptr);

    static std::optional<MatrixVerificationSession>
    fetchVerificationSession(uint64_t handleId, const QString &flowId, QString *errorOut = nullptr);

    static bool
    clearVerificationSession(uint64_t handleId, const QString &flowId, QString *errorOut = nullptr);

    static bool advanceVerificationSession(uint64_t handleId,
                                           const QString &flowId,
                                           QString *errorOut = nullptr);

    static bool cancelVerificationSession(uint64_t handleId,
                                          const QString &flowId,
                                          bool mismatch,
                                          QString *errorOut = nullptr);

    static std::optional<MatrixUserProfile>
    fetchUserProfile(uint64_t handleId, const QString &userId, QString *errorOut = nullptr);

    static std::optional<QVector<MatrixDirectoryUser>> searchUsers(uint64_t handleId,
                                                                   const QString &searchTerm,
                                                                   uint64_t limit,
                                                                   QString *errorOut = nullptr);

    static std::optional<MatrixPublicRoomDirectoryPage>
    fetchPublicRoomDirectoryPage(uint64_t handleId,
                                 const QString &searchTerm,
                                 uint64_t limit,
                                 const QString &since,
                                 const QString &server,
                                 QString *errorOut = nullptr);

    static bool
    setOwnDisplayName(uint64_t handleId, const QString &displayName, QString *errorOut = nullptr);

    static bool uploadOwnAvatar(uint64_t handleId,
                                const QString &filePath,
                                const QString &mimeType,
                                QString *errorOut = nullptr);

    static bool removeOwnAvatar(uint64_t handleId, QString *errorOut = nullptr);

    static bool ignoreUser(uint64_t handleId, const QString &userId, QString *errorOut = nullptr);

    static bool unignoreUser(uint64_t handleId, const QString &userId, QString *errorOut = nullptr);

    static std::optional<QVector<MatrixRoomSummary>>
    fetchRoomList(uint64_t handleId, QString *errorOut = nullptr);
    static void cacheRoomListSnapshot(uint64_t handleId, QVector<MatrixRoomSummary> rooms);
    static void clearCachedRoomListSnapshot(uint64_t handleId);

    static std::optional<MatrixRoomSettings>
    fetchRoomSettings(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static std::optional<QVector<MatrixRoomMember>>
    fetchRoomMembers(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static std::optional<MatrixRoomRedactionPermissions>
    fetchRoomRedactionPermissions(uint64_t handleId,
                                  const QString &roomId,
                                  QString *errorOut = nullptr);

    static bool setRoomNotificationMode(uint64_t handleId,
                                        const QString &roomId,
                                        int mode,
                                        QString *errorOut = nullptr);

    static bool setRoomName(uint64_t handleId,
                            const QString &roomId,
                            const QString &name,
                            QString *errorOut = nullptr);

    static bool setRoomTopic(uint64_t handleId,
                             const QString &roomId,
                             const QString &topic,
                             QString *errorOut = nullptr);

    static bool uploadRoomAvatar(uint64_t handleId,
                                 const QString &roomId,
                                 const QString &filePath,
                                 const QString &mimeType,
                                 int width,
                                 int height,
                                 QString *errorOut = nullptr);

    static bool
    removeRoomAvatar(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static bool
    enableRoomEncryption(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static bool setRoomHistoryVisibility(uint64_t handleId,
                                         const QString &roomId,
                                         const QString &historyVisibility,
                                         QString *errorOut = nullptr);

    static bool setRoomAccessRules(uint64_t handleId,
                                   const QString &roomId,
                                   const QString &joinRule,
                                   bool guestAccess,
                                   const QVector<QString> &allowedRoomIds,
                                   QString *errorOut = nullptr);

    static bool
    selectActiveRoomTimeline(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static bool setActiveRoomTimelineInitialPageSize(uint64_t handleId,
                                                     uint16_t pageSize,
                                                     QString *errorOut = nullptr);

    static std::optional<QVector<MatrixTimelineItem>>
    fetchActiveRoomTimeline(uint64_t handleId, QString *errorOut = nullptr);

    static bool paginateActiveRoomTimelineBackwards(uint64_t handleId,
                                                    uint16_t pageSize,
                                                    QString *errorOut = nullptr);

    static bool sendRoomMessage(uint64_t handleId,
                                const QString &roomId,
                                const QString &body,
                                const QString &formattedHtml,
                                const QString &messageKind,
                                QString *errorOut = nullptr);

    static bool sendRoomReplyMessage(uint64_t handleId,
                                     const QString &roomId,
                                     const QString &repliedToEventId,
                                     const QString &body,
                                     const QString &formattedHtml,
                                     const QString &messageKind,
                                     QString *errorOut = nullptr);

    static bool sendRoomEditMessage(uint64_t handleId,
                                    const QString &roomId,
                                    const QString &targetEventId,
                                    const QString &body,
                                    const QString &formattedHtml,
                                    const QString &messageKind,
                                    QString *errorOut = nullptr);

    static bool toggleRoomReaction(uint64_t handleId,
                                   const QString &roomId,
                                   const QString &eventId,
                                   const QString &reactionKey,
                                   QString *errorOut = nullptr);

    static bool redactRoomEvent(uint64_t handleId,
                                const QString &roomId,
                                const QString &eventId,
                                const QString &reason,
                                QString *errorOut = nullptr);

    static bool markRoomEventAsRead(uint64_t handleId,
                                    const QString &roomId,
                                    const QString &eventId,
                                    QString *errorOut = nullptr);

    static bool reportRoomEvent(uint64_t handleId,
                                const QString &roomId,
                                const QString &eventId,
                                const QString &reason,
                                int score,
                                QString *errorOut = nullptr);

    static std::optional<QStringList>
    fetchRoomPinnedEventIds(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static bool pinRoomEvent(uint64_t handleId,
                             const QString &roomId,
                             const QString &eventId,
                             QString *errorOut = nullptr);

    static bool unpinRoomEvent(uint64_t handleId,
                               const QString &roomId,
                               const QString &eventId,
                               QString *errorOut = nullptr);

    static std::optional<QString> fetchActiveRoomRawEventJson(uint64_t handleId,
                                                              const QString &roomId,
                                                              const QString &eventId,
                                                              QString *errorOut = nullptr);

    static std::optional<QVector<MatrixReadReceiptEntry>>
    fetchRoomReadReceipts(uint64_t handleId,
                          const QString &roomId,
                          const QString &eventId,
                          QString *errorOut = nullptr);

    static bool sendRoomAttachment(uint64_t handleId,
                                   const QString &roomId,
                                   const QString &filePath,
                                   const QString &filename,
                                   const QString &caption,
                                   const QString &replyEventId,
                                   const QString &mimeType,
                                   QString *errorOut = nullptr);

    static std::optional<QByteArray>
    fetchActiveRoomTimelineMediaContent(uint64_t handleId,
                                        const QString &itemId,
                                        int width,
                                        int height,
                                        bool crop,
                                        QString *errorOut = nullptr);

    static std::optional<QByteArray> fetchMediaContent(uint64_t handleId,
                                                       const QString &mxcUri,
                                                       int width,
                                                       int height,
                                                       bool crop,
                                                       QString *errorOut = nullptr);
};

} // namespace komai
