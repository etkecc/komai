// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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
};

} // namespace komai
