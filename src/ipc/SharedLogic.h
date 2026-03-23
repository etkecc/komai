// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>

#include <QImage>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include <QVersionNumber>

namespace komai::ipc {

/// The IPC API version. Kept in sync with komai::dbus::dbusApiVersion in Api.h.
inline const QVersionNumber apiVersionNumber{1, 1, 0};

/// Shared business logic called by both D-Bus adaptors and IPC server.
/// These functions do NOT perform access checks -- callers are responsible.

// -- app --

QString
apiVersion();
QString
appVersion();

// -- rooms --

struct RoomInfo
{
    QString roomId;
    QString alias;
    QString name;
    QString avatarUrl;
    int unreadNotifications = 0;

    QJsonObject toJson() const;
};

QVector<RoomInfo>
roomList();
void
activateRoom(const QString &roomIdOrAlias);
void
joinRoom(const QString &roomIdOrAlias);
void
newDirectChat(const QString &userId);

// -- user --

QString
userId();
QString
homeserverUrl();
QString
deviceId();
QString
statusMessage();
void
setStatusMessage(const QString &message);

// -- settings.ui --

QString
uiTheme();
void
setUiTheme(const QString &theme);

// -- rooms (messaging) --

/// Callback for async send results.
/// On success: eventId is set, error is empty.
/// On failure: eventId is empty, error describes what went wrong.
using SendMessageCallback = std::function<void(const QString &eventId, const QString &error)>;

/// Resolves a room ID or alias to a room ID.
/// If the input starts with '!', returns it directly.
/// If it starts with '#', searches the local cache for a match.
/// Returns empty string if the room is not found among joined rooms.
QString
resolveRoomId(const QString &roomIdOrAlias);

/// Sends a text or notice message to a room.
void
sendMessage(const QString &roomIdOrAlias,
            const QString &body,
            const QString &msgtype,
            const QString &format,
            SendMessageCallback callback);

// -- media --

using MediaFetchCallback = std::function<void(const QImage &)>;
void
mediaFetch(const QString &mxcUri, MediaFetchCallback callback);

struct UploadResult
{
    QString mxcUri;
    QString contentType;
    QString filename;
    uint64_t size = 0;

    QJsonObject toJson() const;
};

using MediaUploadCallback = std::function<void(const UploadResult &result, const QString &error)>;

/// Uploads a file to the homeserver (unencrypted) and returns its mxc:// URI.
void
mediaUpload(const QString &filePath,
            const QString &filename,
            const QString &contentType,
            MediaUploadCallback callback);

} // namespace komai::ipc
