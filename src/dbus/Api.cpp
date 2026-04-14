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
                           const QString &name,
                           const QString &avatarUrl,
                           const bool read,
                           const int serverNotificationCount,
                           const int memberCount,
                           const qulonglong mostRecentEventTimestampMs,
                           const bool highlighted,
                           const QStringList &categories,
                           const QStringList &tags,
                           const QStringList &parentSpaces,
                           const QString &dmUserId,
                           const bool encrypted,
                           QObject *parent)
  : QObject{parent}
  , roomId_{roomId}
  , alias_{alias}
  , name_{name}
  , avatarUrl_{avatarUrl}
  , read_{read}
  , serverNotificationCount_{serverNotificationCount}
  , memberCount_{memberCount}
  , mostRecentEventTimestampMs_{mostRecentEventTimestampMs}
  , highlighted_{highlighted}
  , categories_{categories}
  , tags_{tags}
  , parentSpaces_{parentSpaces}
  , dmUserId_{dmUserId}
  , encrypted_{encrypted}
{
}

RoomInfoItem::RoomInfoItem(const RoomInfoItem &other)
  : QObject{other.parent()}
  , roomId_{other.roomId_}
  , alias_{other.alias_}
  , name_{other.name_}
  , avatarUrl_{other.avatarUrl_}
  , read_{other.read_}
  , serverNotificationCount_{other.serverNotificationCount_}
  , memberCount_{other.memberCount_}
  , mostRecentEventTimestampMs_{other.mostRecentEventTimestampMs_}
  , highlighted_{other.highlighted_}
  , categories_{other.categories_}
  , tags_{other.tags_}
  , parentSpaces_{other.parentSpaces_}
  , dmUserId_{other.dmUserId_}
  , encrypted_{other.encrypted_}
{
}

RoomInfoItem &
RoomInfoItem::operator=(const RoomInfoItem &other)
{
    roomId_                     = other.roomId_;
    alias_                      = other.alias_;
    name_                       = other.name_;
    avatarUrl_                  = other.avatarUrl_;
    read_                       = other.read_;
    serverNotificationCount_    = other.serverNotificationCount_;
    memberCount_                = other.memberCount_;
    mostRecentEventTimestampMs_ = other.mostRecentEventTimestampMs_;
    highlighted_                = other.highlighted_;
    categories_                 = other.categories_;
    tags_                       = other.tags_;
    parentSpaces_               = other.parentSpaces_;
    dmUserId_                   = other.dmUserId_;
    encrypted_                  = other.encrypted_;
    return *this;
}

QDBusArgument &
operator<<(QDBusArgument &arg, const RoomInfoItem &item)
{
    arg.beginStructure();
    arg << item.roomId_ << item.alias_ << item.name_ << item.avatarUrl_ << item.read_
        << item.serverNotificationCount_ << item.memberCount_ << item.mostRecentEventTimestampMs_
        << item.highlighted_ << item.categories_ << item.tags_ << item.parentSpaces_
        << item.dmUserId_ << item.encrypted_;
    arg.endStructure();
    return arg;
}

const QDBusArgument &
operator>>(const QDBusArgument &arg, RoomInfoItem &item)
{
    arg.beginStructure();
    arg >> item.roomId_ >> item.alias_ >> item.name_ >> item.avatarUrl_ >> item.read_ >>
      item.serverNotificationCount_ >> item.memberCount_ >> item.mostRecentEventTimestampMs_ >>
      item.highlighted_ >> item.categories_ >> item.tags_ >> item.parentSpaces_ >> item.dmUserId_ >>
      item.encrypted_;

    arg.endStructure();
    return arg;
}

// -- cc.etke.komai.App --

QString
apiVersion(const QString &profileId)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.App")};
    if (iface.isValid())
        return QDBusReply<QString>{iface.call(QStringLiteral("apiVersion"))}.value();
    return {};
}

QString
appVersion(const QString &profileId)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.App")};
    if (iface.isValid())
        return QDBusReply<QString>{iface.call(QStringLiteral("appVersion"))}.value();
    return {};
}

// -- cc.etke.komai.Rooms --

QVector<RoomInfoItem>
roomList(const QString &profileId)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Rooms")};
    if (iface.isValid())
        return QDBusReply<QVector<RoomInfoItem>>{iface.call(QStringLiteral("list"))}.value();
    return {};
}

QString
roomTimeline(const QString &profileId,
             const QString &roomIdOrAlias,
             const int limit,
             const QString &beforeEventId,
             const bool includeUnsignedFields,
             const QString &fetchMode)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Rooms")};
    if (iface.isValid()) {
        const auto reply = QDBusReply<QString>{iface.call(QStringLiteral("timeline"),
                                                          roomIdOrAlias,
                                                          limit,
                                                          beforeEventId,
                                                          includeUnsignedFields,
                                                          fetchMode)};
        if (reply.isValid())
            return reply.value();
    }
    return {};
}

void
joinRoom(const QString &profileId, const QString &roomIdOrAlias)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Rooms")};
    if (iface.isValid())
        iface.call(QDBus::NoBlock, QStringLiteral("join"), roomIdOrAlias);
}

void
newDirectChat(const QString &profileId, const QString &userId)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Rooms")};
    if (iface.isValid())
        iface.call(QDBus::NoBlock, QStringLiteral("newDirectChat"), userId);
}

QString
sendMessage(const QString &profileId,
            const QString &roomIdOrAlias,
            const QString &body,
            const QString &msgtype,
            const QString &format)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Rooms")};
    if (iface.isValid())
        return QDBusReply<QString>{
          iface.call(QStringLiteral("send"), roomIdOrAlias, body, msgtype, format)}
          .value();
    return {};
}

QString
sendImageFromFile(const QString &profileId,
                  const QString &roomIdOrAlias,
                  const QString &filePath,
                  const QString &body)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Rooms")};
    if (iface.isValid())
        return QDBusReply<QString>{
          iface.call(QStringLiteral("sendImageFile"), roomIdOrAlias, filePath, body)}
          .value();
    return {};
}

QString
sendImage(const QString &profileId,
          const QString &roomIdOrAlias,
          const QString &mxcUri,
          const QString &body,
          const QString &filename)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Rooms")};
    if (iface.isValid())
        return QDBusReply<QString>{
          iface.call(QStringLiteral("sendImage"), roomIdOrAlias, mxcUri, body, filename)}
          .value();
    return {};
}

// -- cc.etke.komai.User --

QString
userId(const QString &profileId)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.User")};
    if (iface.isValid())
        return QDBusReply<QString>{iface.call(QStringLiteral("userId"))}.value();
    return {};
}

QString
homeserverUrl(const QString &profileId)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.User")};
    if (iface.isValid())
        return QDBusReply<QString>{iface.call(QStringLiteral("homeserverUrl"))}.value();
    return {};
}

QString
deviceId(const QString &profileId)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.User")};
    if (iface.isValid())
        return QDBusReply<QString>{iface.call(QStringLiteral("deviceId"))}.value();
    return {};
}

QString
statusMessage(const QString &profileId)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.User")};
    if (iface.isValid())
        return QDBusReply<QString>{iface.call(QStringLiteral("statusMessage"))}.value();
    return {};
}

void
setStatusMessage(const QString &profileId, const QString &message)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.User")};
    if (iface.isValid())
        iface.call(QDBus::NoBlock, QStringLiteral("setStatusMessage"), message);
}

// -- cc.etke.komai.Settings.UI --

QString
theme(const QString &profileId)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Settings.UI")};
    if (iface.isValid())
        return QDBusReply<QString>{iface.call(QStringLiteral("theme"))}.value();
    return {};
}

void
setTheme(const QString &profileId, const QString &theme)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Settings.UI")};
    if (iface.isValid())
        iface.call(QDBus::NoBlock, QStringLiteral("setTheme"), theme);
}

// -- cc.etke.komai.Media --

QImage
mediaFetch(const QString &profileId, const QString &mxcUri)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Media")};
    if (iface.isValid())
        return QDBusReply<QImage>{iface.call(QStringLiteral("fetch"), mxcUri)}.value();
    return {};
}

QString
mediaUpload(const QString &profileId,
            const QString &filePath,
            const QString &filename,
            const QString &contentType)
{
    QDBusInterface iface{
      serviceName(profileId), QStringLiteral("/"), QStringLiteral("cc.etke.komai.Media")};
    if (iface.isValid())
        return QDBusReply<QString>{
          iface.call(QStringLiteral("upload"), filePath, filename, contentType)}
          .value();
    return {};
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
