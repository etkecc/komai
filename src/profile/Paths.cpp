// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile/Paths.h"

#include <QByteArray>
#include <QSet>
#include <QStandardPaths>

#include "profile/ProfileId.h"

namespace {
QString
rootWithAppName(QStandardPaths::StandardLocation location)
{
    return QStandardPaths::writableLocation(location) + QStringLiteral("/komai");
}

QString
escapedStorageComponent(QStringView value)
{
    static constexpr char hexDigits[] = "0123456789ABCDEF";
    const QByteArray bytes            = value.toString().toUtf8();

    QString escaped;
    escaped.reserve(bytes.size() * 3);

    for (const auto byte : bytes) {
        const auto ch     = static_cast<unsigned char>(byte);
        const bool isSafe = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z') ||
                            (ch >= 'A' && ch <= 'Z') || ch == '-' || ch == '_' || ch == '.' ||
                            ch == '@' || ch == '+';

        if (isSafe) {
            escaped.append(QChar::fromLatin1(static_cast<char>(ch)));
            continue;
        }

        escaped.append(QChar::fromLatin1('%'));
        escaped.append(QChar::fromLatin1(hexDigits[(ch >> 4) & 0x0f]));
        escaped.append(QChar::fromLatin1(hexDigits[ch & 0x0f]));
    }

    if (escaped.isEmpty())
        return QStringLiteral("_");

    if (escaped.endsWith(QLatin1Char('.'))) {
        escaped.chop(1);
        escaped.append(QStringLiteral("%2E"));
    }

    return escaped;
}
}

namespace app_paths {

QString
normalizedProfileId(QStringView profileId)
{
    return profile_id::normalized(profileId);
}

QString
encodedIdComponent(QStringView value)
{
    return QString::fromUtf8(value.toString().toUtf8().toBase64(QByteArray::Base64UrlEncoding |
                                                                QByteArray::OmitTrailingEquals));
}

namespace config {

QString
root()
{
    return rootWithAppName(QStandardPaths::GenericConfigLocation);
}

QString
profileConfigFile(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId) +
           QStringLiteral("/config.yml");
}

QString
profileStateFile(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId) +
           QStringLiteral("/state.yml");
}

QString
profileSessionFile(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId) +
           QStringLiteral("/session.yml");
}

QString
profileSecretsFile(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId) +
           QStringLiteral("/secrets.yml");
}

} // namespace config

namespace data {

QString
root()
{
    return rootWithAppName(QStandardPaths::GenericDataLocation);
}

QString
dbRoot(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId) +
           QStringLiteral("/db");
}

QString
databaseDirectory(QStringView userId, QStringView profileId)
{
    return dbRoot(profileId) + QStringLiteral("/") + escapedStorageComponent(userId);
}

QString
userThemesDirectory()
{
    return root() + QStringLiteral("/themes");
}

QStringList
themeSearchDirectories()
{
    const auto locations = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
    QStringList result;
    QSet<QString> seen;

    for (const auto &location : locations) {
        const auto dir = location + QStringLiteral("/komai/themes");
        if (seen.contains(dir))
            continue;
        result.push_back(dir);
        seen.insert(dir);
    }

    return result;
}

} // namespace data

namespace cache {

QString
root()
{
    return rootWithAppName(QStandardPaths::GenericCacheLocation);
}

QString
profileDirectory(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId);
}

QString
mediaRoot(QStringView profileId)
{
    return profileDirectory(profileId) + QStringLiteral("/media");
}

QString
roomMediaDirectory(QStringView profileId, QStringView roomId)
{
    return mediaRoot(profileId) + QStringLiteral("/rooms/") + encodedIdComponent(roomId);
}

QString
sharedMediaDirectory(QStringView profileId)
{
    return mediaRoot(profileId) + QStringLiteral("/shared");
}

static QString
mediaDirectoryForContext(QStringView profileId, QStringView roomId)
{
    if (roomId.isEmpty())
        return sharedMediaDirectory(profileId);
    return roomMediaDirectory(profileId, roomId);
}

QString
mediaFileForMxc(QStringView profileId, QStringView mxcId, QStringView suffix, QStringView roomId)
{
    return QStringLiteral("%1/full/%2.%3")
      .arg(
        mediaDirectoryForContext(profileId, roomId), encodedIdComponent(mxcId), suffix.toString());
}

QString
mediaFullFileForMxc(QStringView profileId, QStringView mxcId, QStringView roomId)
{
    return QStringLiteral("%1/full/%2")
      .arg(mediaDirectoryForContext(profileId, roomId), encodedIdComponent(mxcId));
}

QString
mediaThumbnailFileForMxc(QStringView profileId,
                         QStringView mxcId,
                         const QSize &requestedSize,
                         bool crop,
                         double radius,
                         QStringView roomId)
{
    return QStringLiteral("%1/thumbnails/%2_%3x%4_%5_radius%6")
      .arg(mediaDirectoryForContext(profileId, roomId),
           encodedIdComponent(mxcId),
           QString::number(requestedSize.width()),
           QString::number(requestedSize.height()),
           crop ? QStringLiteral("crop") : QStringLiteral("scale"),
           QString::number(radius));
}

QString
roomNotificationAvatarFile(QStringView profileId, QStringView roomId)
{
    return QStringLiteral("%1/notifications/room-avatar-%2.png")
      .arg(profileDirectory(profileId), encodedIdComponent(roomId));
}

QString
logDirectory(QStringView profileId)
{
    return profileDirectory(profileId) + QStringLiteral("/logs");
}

QString
logFile(QStringView profileId)
{
    return logDirectory(profileId) + QStringLiteral("/komai.log");
}

QString
httpCacheDirectory(QStringView profileId)
{
    return profileDirectory(profileId) + QStringLiteral("/http");
}

QString
altSvcCacheFile(QStringView profileId)
{
    return httpCacheDirectory(profileId) + QStringLiteral("/alt_svc_cache.txt");
}

} // namespace cache

} // namespace app_paths
