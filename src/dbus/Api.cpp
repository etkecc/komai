// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Api.h"

#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>

#include "profile/ProfileId.h"

namespace komai::dbus {
void
init()
{
    qDBusRegisterMetaType<RoomInfoItem>();
    qDBusRegisterMetaType<QVector<RoomInfoItem>>();
    qDBusRegisterMetaType<QImage>();
}

QString
serviceName(const QString &profileId)
{
    QString normalized = profile_id::normalized(profileId);

    // D-Bus name elements: [A-Za-z_][A-Za-z0-9_]* — dots separate elements, so replace them.
    QString sanitized = normalized.replace(QLatin1Char('.'), QLatin1Char('_'));

    // D-Bus element must not start with a digit.
    if (!sanitized.isEmpty() && sanitized.front().isDigit())
        sanitized.prepend(QLatin1Char('_'));

    return QLatin1String(dbusServicePrefix) + sanitized;
}

QStringList
runningProfiles()
{
    QStringList profiles;
    auto *iface = QDBusConnection::sessionBus().interface();
    if (!iface)
        return profiles;

    const QDBusReply<QStringList> reply = iface->registeredServiceNames();
    if (!reply.isValid())
        return profiles;

    const QString prefix = QLatin1String(dbusServicePrefix);
    for (const QString &name : reply.value()) {
        if (name.startsWith(prefix))
            profiles.append(name.mid(prefix.size()));
    }
    return profiles;
}

bool
apiVersionIsCompatible(const QVersionNumber &clientAppVersion)
{
    if (clientAppVersion.majorVersion() != komai::dbus::dbusApiVersion.majorVersion())
        return false;
    if (clientAppVersion.minorVersion() > komai::dbus::dbusApiVersion.minorVersion())
        return false;
    if (clientAppVersion.minorVersion() == komai::dbus::dbusApiVersion.minorVersion() &&
        clientAppVersion.microVersion() < komai::dbus::dbusApiVersion.microVersion())
        return false;

    return true;
}

RoomInfoItem::RoomInfoItem(const QString &roomId,
                           const QString &alias,
                           const QString &title,
                           const QString &avatarUrl,
                           const int unreadNotifications,
                           QObject *parent)
  : QObject{parent}
  , roomId_{roomId}
  , alias_{alias}
  , roomName_{title}
  , avatarUrl_{avatarUrl}
  , unreadNotifications_{unreadNotifications}
{
}

RoomInfoItem::RoomInfoItem(const RoomInfoItem &other)
  : QObject{other.parent()}
  , roomId_{other.roomId_}
  , alias_{other.alias_}
  , roomName_{other.roomName_}
  , avatarUrl_{other.avatarUrl_}
  , unreadNotifications_{other.unreadNotifications_}
{
}

RoomInfoItem &
RoomInfoItem::operator=(const RoomInfoItem &other)
{
    roomId_              = other.roomId_;
    alias_               = other.alias_;
    roomName_            = other.roomName_;
    avatarUrl_           = other.avatarUrl_;
    unreadNotifications_ = other.unreadNotifications_;
    return *this;
}

QDBusArgument &
operator<<(QDBusArgument &arg, const RoomInfoItem &item)
{
    arg.beginStructure();
    arg << item.roomId_ << item.alias_ << item.roomName_ << item.avatarUrl_
        << item.unreadNotifications_;
    arg.endStructure();
    return arg;
}

const QDBusArgument &
operator>>(const QDBusArgument &arg, RoomInfoItem &item)
{
    arg.beginStructure();
    arg >> item.roomId_ >> item.alias_ >> item.roomName_ >> item.avatarUrl_ >>
      item.unreadNotifications_;

    arg.endStructure();
    return arg;
}

QString
apiVersion(const QString &profileId)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        return QDBusReply<QString>{interface.call(QStringLiteral("apiVersion"))}.value();
    else
        return {};
}

QString
appVersion(const QString &profileId)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        return QDBusReply<QString>{interface.call(QStringLiteral("appVersion"))}.value();
    else
        return {};
}

QVector<RoomInfoItem>
rooms(const QString &profileId)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        return QDBusReply<QVector<RoomInfoItem>>{interface.call(QStringLiteral("rooms"))}.value();
    else
        return {};
}

QImage
image(const QString &profileId, const QString &mxcuri)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        return QDBusReply<QImage>{interface.call(QStringLiteral("image"), mxcuri)}.value();
    else
        return {};
}

void
activateRoom(const QString &profileId, const QString &alias)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        interface.call(QDBus::NoBlock, QStringLiteral("activateRoom"), alias);
}

void
joinRoom(const QString &profileId, const QString &alias)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        interface.call(QDBus::NoBlock, QStringLiteral("joinRoom"), alias);
}

void
directChat(const QString &profileId, const QString &userId)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        interface.call(QDBus::NoBlock, QStringLiteral("directChat"), userId);
}

QString
statusMessage(const QString &profileId)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        return QDBusReply<QString>{interface.call(QStringLiteral("statusMessage"))}.value();
    else
        return {};
}

void
setStatusMessage(const QString &profileId, const QString &message)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        interface.call(QDBus::NoBlock, QStringLiteral("setStatusMessage"), message);
}

void
setTheme(const QString &profileId, const QString &theme)
{
    if (QDBusInterface interface{serviceName(profileId), QStringLiteral("/")}; interface.isValid())
        interface.call(QDBus::NoBlock, QStringLiteral("setTheme"), theme);
}

} // komai::dbus

/**
 * Automatic marshaling of a QImage for org.freedesktop.Notifications.Notify
 *
 * This function is heavily based on a function from the Clementine project (see
 * http://www.clementine-player.org) and licensed under the GNU General Public
 * License, version 3 or later.
 *
 */
QDBusArgument &
operator<<(QDBusArgument &arg, const QImage &image)
{
    if (image.isNull()) {
        arg.beginStructure();
        arg << 0 << 0 << 0 << false << 0 << 0 << QByteArray();
        arg.endStructure();
        return arg;
    }

    QImage i      = image.height() > 100 || image.width() > 100
                      ? image.scaledToHeight(100, Qt::SmoothTransformation)
                      : image;
    bool hasAlpha = i.hasAlphaChannel();
    i = std::move(i).convertToFormat(hasAlpha ? QImage::Format_RGBA8888 : QImage::Format_RGB888);

    int channels = hasAlpha ? 4 : 3;
    QByteArray arr(reinterpret_cast<const char *>(i.bits()), static_cast<int>(i.sizeInBytes()));
    arg.beginStructure();
    arg << i.width() << i.height() << (int)i.bytesPerLine() << i.hasAlphaChannel()
        << i.depth() / channels << channels << arr;
    arg.endStructure();

    return arg;
}

// This function, however, was merely reverse-engineered from the above function
// and is not from the Clementine project.
const QDBusArgument &
operator>>(const QDBusArgument &arg, QImage &image)
{
    // garbage is used as a sort of /dev/null
    int width, height, garbage;
    QByteArray bits;

    arg.beginStructure();
    arg >> width >> height >> garbage >> garbage >> garbage >> garbage >> bits;
    arg.endStructure();

    // Unfortunately, this copy-and-detach is necessary to ensure that the source buffer
    // is copied properly. If anybody finds a better solution, please implement it.
    auto temp =
      QImage(reinterpret_cast<uchar *>(bits.data()), width, height, QImage::Format_RGBA8888);
    image = temp;
    image.detach();

    return arg;
}
