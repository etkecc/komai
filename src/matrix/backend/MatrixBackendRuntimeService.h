// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>
#include <optional>
#include <utility>

#include "matrix/backend/MatrixBackendRuntimeServiceTypes.h"
#include "matrix/backend/MatrixBlockingCall.h"
#include "timeline/Reaction.h"
#include "voip/CallTypes.h"

namespace komai {

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

    /// Returns the new room's id on success.  `additionalCreators` is only
    /// honored by the server from room version 12 onwards.
    static std::optional<QString> upgradeRoom(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &roomId,
                                              const QString &newVersion,
                                              const QStringList &additionalCreators,
                                              QString *errorOut = nullptr);

    static bool setUserPowerLevel(matrix_backend::BlockingCallContext context,
                                  uint64_t handleId,
                                  const QString &roomId,
                                  const QString &userId,
                                  int64_t powerLevel,
                                  QString *errorOut = nullptr);

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

    // Returns the number of exported keys.
    static std::optional<uint64_t> exportRoomKeys(matrix_backend::BlockingCallContext context,
                                                  uint64_t handleId,
                                                  const QString &path,
                                                  const QString &passphrase,
                                                  QString *errorOut = nullptr);

    static std::optional<MatrixRoomKeyImportCounts>
    importRoomKeys(matrix_backend::BlockingCallContext context,
                   uint64_t handleId,
                   const QString &path,
                   const QString &passphrase,
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

    static std::optional<MatrixRoomVersionsCapability>
    fetchRoomVersionsCapability(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
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

    static QString uploadRoomAvatar(matrix_backend::BlockingCallContext context,
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
    subscribeToRoom(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static bool
    unsubscribeFromRoom(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

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
                                const QString &mentionUserIds = QString(),
                                bool mentionsRoom             = false,
                                QString *errorOut             = nullptr);
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
                                     const QString &threadId       = QString(),
                                     const QString &mentionUserIds = QString(),
                                     bool mentionsRoom             = false,
                                     QString *errorOut             = nullptr);

    static bool sendRoomEditMessage(matrix_backend::BlockingCallContext context,
                                    uint64_t handleId,
                                    const QString &roomId,
                                    const QString &targetEventId,
                                    const QString &body,
                                    bool useMarkdownFormatting,
                                    const QString &messageKind,
                                    const QString &mentionUserIds = QString(),
                                    bool mentionsRoom             = false,
                                    QString *errorOut             = nullptr);

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

    static bool cancelRoomLocalEcho(matrix_backend::BlockingCallContext context,
                                    uint64_t handleId,
                                    const QString &roomId,
                                    const QString &transactionId,
                                    QString *errorOut = nullptr);

    static bool retryRoomLocalEcho(matrix_backend::BlockingCallContext context,
                                   uint64_t handleId,
                                   const QString &roomId,
                                   const QString &transactionId,
                                   QString *errorOut = nullptr);

    static bool markRoomEventAsRead(matrix_backend::BlockingCallContext context,
                                    uint64_t handleId,
                                    const QString &roomId,
                                    const QString &eventId,
                                    bool publicReceipt,
                                    QString *errorOut = nullptr);

    static bool markRoomAsRead(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &roomId,
                               bool publicReceipt,
                               QString *errorOut = nullptr);

    static bool markRoomUnread(matrix_backend::BlockingCallContext context,
                               uint64_t handleId,
                               const QString &roomId,
                               bool unread,
                               QString *errorOut = nullptr);

    static bool reportRoomEvent(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
                                const QString &roomId,
                                const QString &eventId,
                                const QString &reason,
                                QString *errorOut = nullptr);

    struct ThreadRootsResult
    {
        QVariantList items;
        QString nextBatchToken;
    };

    static std::optional<ThreadRootsResult>
    fetchRoomThreadRoots(matrix_backend::BlockingCallContext context,
                         uint64_t handleId,
                         const QString &roomId,
                         const QString &include,
                         const QString &from,
                         uint32_t limit,
                         QString *errorOut = nullptr);

    static std::optional<QVector<MatrixTimelineItem>>
    fetchThreadTimelineSnapshot(matrix_backend::BlockingCallContext context,
                                uint64_t handleId,
                                QString *errorOut = nullptr);

    static std::optional<bool>
    paginateThreadTimelineBackwards(matrix_backend::BlockingCallContext context,
                                    uint64_t handleId,
                                    uint16_t numEvents,
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
        // Cleartext (post-decryption for encrypted events, identical to the
        // wire form for plaintext events). Empty when `cleartextError` is set.
        QString cleartextJson;
        // Populated when cleartext can't be produced — typically a UTD.
        QString cleartextError;
        // What the homeserver actually delivered. Empty when `wireError` is set.
        QString wireJson;
        QString wireError;
        // True when the wire form is byte-equivalent to the cleartext (i.e. the
        // event was sent in the clear). Drives the "(same)" annotation in the
        // dialog's wire-form segment.
        bool wireMatchesCleartext = false;
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
                                   bool useMarkdownFormatting,
                                   const QString &replyEventId,
                                   const QString &threadId,
                                   const QString &mimeType,
                                   uint64_t durationMs          = 0,
                                   bool isVoice                 = false,
                                   const QList<float> &waveform = {},
                                   bool stripImageMetadata      = true,
                                   QString *errorOut            = nullptr);

    static std::optional<QString> uploadMedia(matrix_backend::BlockingCallContext context,
                                              uint64_t handleId,
                                              const QString &filePath,
                                              const QString &mimeType,
                                              bool stripImageMetadata = true,
                                              QString *errorOut       = nullptr);

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

    // Full-size variant of fetchActiveRoomTimelineMediaContent whose download
    // publishes (received, total) progress, readable while the call is in
    // flight via activeTimelineMediaDownloadProgress().
    static std::optional<QByteArray>
    fetchActiveRoomTimelineMediaContentWithProgress(matrix_backend::BlockingCallContext context,
                                                    uint64_t handleId,
                                                    const QString &itemId,
                                                    QString *errorOut = nullptr);

    // (receivedBytes, totalBytes) of an in-flight progress-tracked media
    // download; (0, 0) when none is running or the total is still unknown.
    static std::pair<uint64_t, uint64_t>
    activeTimelineMediaDownloadProgress(uint64_t handleId, const QString &itemId);

    static std::optional<QByteArray> fetchMediaContent(matrix_backend::BlockingCallContext context,
                                                       uint64_t handleId,
                                                       const QString &mxcUri,
                                                       int width,
                                                       int height,
                                                       bool crop,
                                                       QString *errorOut = nullptr);
};

} // namespace komai
