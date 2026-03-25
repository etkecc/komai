// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>
#include <cstdint>
#include <optional>

namespace komai {

struct MatrixBackendHandleInfo
{
    uint64_t handleId = 0;
    bool hasSession   = false;
    QString homeserverUrl;
    QString userId;
    QString deviceId;
};

struct MatrixOwnProfile
{
    QString displayName;
    QString avatarUrl;
};

struct MatrixUserProfile
{
    QString displayName;
    QString avatarUrl;
};

struct MatrixRoomSummary
{
    QString roomId;
    QString displayName;
    QString avatarUrl;
    QString topic;
    QString lastMessage;
    QString lastMessageKind;
    QString directChatOtherUserId;
    bool isInvite              = false;
    bool isSpace               = false;
    bool isDirect              = false;
    bool isBotRoom             = false;
    bool isEncrypted           = false;
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

struct MatrixTimelineItem
{
    QString itemId;
    QString eventId;
    QString senderId;
    QString senderDisplayName;
    QString senderAvatarUrl;
    QString body;
    QString itemKind;
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

    static std::optional<MatrixUserProfile>
    fetchUserProfile(uint64_t handleId, const QString &userId, QString *errorOut = nullptr);

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

    static std::optional<MatrixRoomSettings>
    fetchRoomSettings(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

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

    static bool sendRoomAttachment(uint64_t handleId,
                                   const QString &roomId,
                                   const QString &filePath,
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
