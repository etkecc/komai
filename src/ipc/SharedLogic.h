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
inline const QVersionNumber apiVersionNumber{1, 0, 2};

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

// -- media --

using MediaFetchCallback = std::function<void(const QImage &)>;
void
mediaFetch(const QString &mxcUri, MediaFetchCallback callback);

} // namespace komai::ipc
