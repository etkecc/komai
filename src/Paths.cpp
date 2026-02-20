// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Paths.h"

#include <QByteArray>
#include <QCryptographicHash>
#include <QSet>
#include <QStandardPaths>

#include "ProfileSecrets.h"

namespace {
QString
rootWithAppName(QStandardPaths::StandardLocation location)
{
    return QStandardPaths::writableLocation(location) + QStringLiteral("/komai");
}
}

namespace app_paths {

QString
normalizedProfileId(QStringView profileId)
{
    return profile_secrets::normalizedProfileId(profileId);
}

QString
encodedIdComponent(QStringView value)
{
    return QString::fromUtf8(value.toString().toUtf8().toBase64(
      QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
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
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(userId.toString().toUtf8());

    return dbRoot(profileId) + QStringLiteral("/") + QString::fromLatin1(hash.result().toHex());
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
mediaDirectory(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId) +
           QStringLiteral("/media_cache");
}

QString
mediaMediaDirectory(QStringView profileId)
{
    return mediaDirectory(profileId) + QStringLiteral("/media");
}

QString
mediaFileForMxc(QStringView profileId, QStringView mxcId, QStringView suffix)
{
    return QStringLiteral("%1/%2.%3")
      .arg(mediaDirectory(profileId), encodedIdComponent(mxcId), suffix.toString());
}

QString
mediaMediaFileForMxc(QStringView profileId, QStringView mxcId, QStringView suffix)
{
    return QStringLiteral("%1/%2.%3")
      .arg(mediaMediaDirectory(profileId), encodedIdComponent(mxcId), suffix.toString());
}

QString
mediaThumbnailFileForMxc(QStringView profileId,
                         QStringView mxcId,
                         const QSize &requestedSize,
                         bool crop,
                         double radius)
{
    return QStringLiteral("%1/%2_%3x%4_%5_radius%6")
      .arg(mediaDirectory(profileId),
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
      .arg(root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId),
           encodedIdComponent(roomId));
}

QString
logFile(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId) +
           QStringLiteral("/komai.log");
}

QString
altSvcCacheFile(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId) +
           QStringLiteral("/curl_alt_svc_cache.txt");
}

} // namespace cache

} // namespace app_paths
