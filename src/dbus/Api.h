// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QDBusArgument>
#include <QIcon>
#include <QObject>
#include <QStringList>
#include <QVersionNumber>

namespace komai::dbus {

//! Registers all necessary classes with D-Bus. Call this before using any Komai D-Bus classes.
void
init();

//! The Komai D-Bus API version provided by this file. The API version number follows semantic
//! versioning as defined by https://semver.org.
inline const QVersionNumber dbusApiVersion{1, 1, 0};

//! Compare the installed API version to the version that your client app targets to see if they
//! are compatible.
bool
apiVersionIsCompatible(const QVersionNumber &clientAppVersion);

//! Returns the D-Bus service name for the given profile.
//! An empty or "default" profile maps to "cc.etke.komai.profile.default".
QString
serviceName(const QString &profileId);

//! Returns profile IDs for all Komai instances currently registered on the session bus.
QStringList
runningProfiles();

class RoomInfoItem final : public QObject
{
    Q_OBJECT

public:
    RoomInfoItem(const QString &roomId                       = QString{},
                 const QString &alias                        = QString{},
                 const QString &name                         = QString{},
                 const QString &avatarUrl                    = QString{},
                 const bool read                             = true,
                 const int unreadCount                       = 0,
                 const int memberCount                       = 0,
                 const qulonglong mostRecentEventTimestampMs = 0,
                 const bool highlighted                      = false,
                 const QStringList &categories               = {},
                 const QStringList &tags                     = {},
                 const QStringList &parentSpaces             = {},
                 const QString &dmUserId                     = QString{},
                 const bool encrypted                        = false,
                 QObject *parent                             = nullptr);

    RoomInfoItem(const RoomInfoItem &other);

    const QString &roomId() const { return roomId_; }
    const QString &alias() const { return alias_; }
    const QString &name() const { return name_; }
    const QString &avatarUrl() const { return avatarUrl_; }
    bool read() const { return read_; }
    int unreadCount() const { return unreadCount_; }
    int memberCount() const { return memberCount_; }
    qulonglong mostRecentEventTimestampMs() const { return mostRecentEventTimestampMs_; }
    bool highlighted() const { return highlighted_; }
    const QStringList &categories() const { return categories_; }
    const QStringList &tags() const { return tags_; }
    const QStringList &parentSpaces() const { return parentSpaces_; }
    const QString &dmUserId() const { return dmUserId_; }
    bool encrypted() const { return encrypted_; }

    RoomInfoItem &operator=(const RoomInfoItem &other);
    friend QDBusArgument &operator<<(QDBusArgument &arg, const komai::dbus::RoomInfoItem &item);
    friend const QDBusArgument &
    operator>>(const QDBusArgument &arg, komai::dbus::RoomInfoItem &item);

private:
    QString roomId_;
    QString alias_;
    QString name_;
    QString avatarUrl_;
    bool read_;
    int unreadCount_;
    int memberCount_;
    qulonglong mostRecentEventTimestampMs_;
    bool highlighted_;
    QStringList categories_;
    QStringList tags_;
    QStringList parentSpaces_;
    QString dmUserId_;
    bool encrypted_;
};

// -- cc.etke.komai.App --

//! Get the Komai D-Bus API version.
QString
apiVersion(const QString &profileId);
//! Get the app version.
QString
appVersion(const QString &profileId);

// -- cc.etke.komai.Rooms --

//! Get a list of all joined rooms.
QVector<RoomInfoItem>
roomList(const QString &profileId);
//! Read visible timeline events from a room. Returns a JSON object string.
QString
roomTimeline(const QString &profileId,
             const QString &roomIdOrAlias,
             int limit                    = 10,
             const QString &beforeEventId = {},
             bool includeUnsignedFields   = false,
             const QString &fetchMode     = {});
//! Joins a room.
void
joinRoom(const QString &profileId, const QString &roomIdOrAlias);
//! Starts or activates a direct chat.
void
newDirectChat(const QString &profileId, const QString &userId);
//! Sends a text or notice message to a room. Returns the event ID.
QString
sendMessage(const QString &profileId,
            const QString &roomIdOrAlias,
            const QString &body,
            const QString &msgtype = QStringLiteral("m.text"),
            const QString &format  = QStringLiteral("auto"));
//! Uploads an image from disk and sends it to a room. Handles encryption.
QString
sendImageFromFile(const QString &profileId,
                  const QString &roomIdOrAlias,
                  const QString &filePath,
                  const QString &body = {});
//! Sends an image using an already-uploaded mxc:// URI (unencrypted rooms only).
QString
sendImage(const QString &profileId,
          const QString &roomIdOrAlias,
          const QString &mxcUri,
          const QString &body     = {},
          const QString &filename = {});

// -- cc.etke.komai.User --

//! Get the logged-in user's Matrix ID.
QString
userId(const QString &profileId);
//! Get the homeserver URL.
QString
homeserverUrl(const QString &profileId);
//! Get the device ID.
QString
deviceId(const QString &profileId);
//! Get the user's status message.
QString
statusMessage(const QString &profileId);
//! Sets the user's status message.
void
setStatusMessage(const QString &profileId, const QString &message);

// -- cc.etke.komai.Settings.UI --

//! Get the current theme slug.
QString
theme(const QString &profileId);
//! Sets the current theme.
void
setTheme(const QString &profileId, const QString &theme);

// -- cc.etke.komai.Media --

//! Fetch an image using a matrix content URI.
QImage
mediaFetch(const QString &profileId, const QString &mxcUri);
//! Uploads a file (unencrypted) and returns its mxc:// URI.
QString
mediaUpload(const QString &profileId,
            const QString &filePath,
            const QString &filename    = {},
            const QString &contentType = {});

QDBusArgument &
operator<<(QDBusArgument &arg, const RoomInfoItem &item);
const QDBusArgument &
operator>>(const QDBusArgument &arg, RoomInfoItem &item);
} // komai::dbus

QDBusArgument &
operator<<(QDBusArgument &arg, const QImage &image);
const QDBusArgument &
operator>>(const QDBusArgument &arg, QImage &);

inline constexpr auto dbusServicePrefix = "cc.etke.komai.profile.";
