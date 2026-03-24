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

struct MatrixRoomSummary
{
    QString roomId;
    QString displayName;
    QString avatarUrl;
    QString topic;
    bool isInvite              = false;
    bool isSpace               = false;
    bool isDirect              = false;
    bool isEncrypted           = false;
    uint64_t unreadMessages    = 0;
    uint64_t notificationCount = 0;
    uint64_t highlightCount    = 0;
    uint64_t timestamp         = 0;
};

struct MatrixTimelineItem
{
    QString itemId;
    QString eventId;
    QString senderId;
    QString senderDisplayName;
    QString body;
    QString itemKind;
    uint64_t timestamp = 0;
    bool isOwn         = false;

    bool operator==(const MatrixTimelineItem &) const = default;
};

class MatrixBackendRuntimeService
{
public:
    static std::optional<MatrixBackendHandleInfo>
    startRestoredBackend(const QString &profileId, QString *errorOut = nullptr);

    static bool stopBackend(uint64_t handleId, QString *errorOut = nullptr);
    static bool startSync(uint64_t handleId, QString *errorOut = nullptr);

    static std::optional<MatrixOwnProfile>
    fetchOwnProfile(uint64_t handleId, QString *errorOut = nullptr);

    static std::optional<QVector<MatrixRoomSummary>>
    fetchRoomList(uint64_t handleId, QString *errorOut = nullptr);

    static bool
    selectActiveRoomTimeline(uint64_t handleId, const QString &roomId, QString *errorOut = nullptr);

    static std::optional<QVector<MatrixTimelineItem>>
    fetchActiveRoomTimeline(uint64_t handleId, QString *errorOut = nullptr);

    static std::optional<QByteArray> fetchMediaContent(uint64_t handleId,
                                                       const QString &mxcUri,
                                                       int width,
                                                       int height,
                                                       bool crop,
                                                       QString *errorOut = nullptr);
};

} // namespace komai
