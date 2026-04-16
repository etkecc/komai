// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "profile/Paths.h"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>

#include "config/komai.h"
#include "profile/KeyringEnvironment.h"
#include "profile/ProfileId.h"

namespace {
QString
rootWithAppName(QStandardPaths::StandardLocation location)
{
    return QStandardPaths::writableLocation(location) + QStringLiteral("/komai");
}

QStringList
desktopApplicationsSearchDirectories()
{
    QStringList result;
    QSet<QString> seen;

    for (const auto &location :
         QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation)) {
        if (location.isEmpty() || seen.contains(location))
            continue;
        result.push_back(location);
        seen.insert(location);
    }

    const auto writableApplicationsDir =
      QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (!writableApplicationsDir.isEmpty() && !seen.contains(writableApplicationsDir)) {
        result.push_front(writableApplicationsDir);
        seen.insert(writableApplicationsDir);
    }

    return result;
}

bool
desktopExecArgNeedsQuotes(QStringView value)
{
    if (value.isEmpty())
        return true;

    static const QRegularExpression reservedChars{
      QStringLiteral(R"([ \t\n"\'\\><~|&;\$\*\?#\(\)`])")};
    return reservedChars.matchView(value).hasMatch();
}

QString
desktopExecArg(QStringView value)
{
    const auto literal = value.toString();
    if (!desktopExecArgNeedsQuotes(literal))
        return literal;

    QString escaped;
    escaped.reserve(literal.size() * 2);

    for (const QChar ch : literal) {
        if (ch == QLatin1Char('\\')) {
            escaped += QStringLiteral("\\\\\\\\");
            continue;
        }
        if (ch == QLatin1Char('$')) {
            escaped += QStringLiteral("\\\\$");
            continue;
        }
        if (ch == QLatin1Char('"') || ch == QLatin1Char('`'))
            escaped += QLatin1Char('\\');
        escaped += ch;
    }

    return QStringLiteral("\"%1\"").arg(escaped);
}

QString
profileDesktopEntryContents(QStringView profileId, QStringView executablePath)
{
    const auto normalizedProfile = app_paths::normalizedProfileId(profileId);
    return QStringLiteral("[Desktop Entry]\n"
                          "Version=1.5\n"
                          "Type=Application\n"
                          "Name=Komai (%1)\n"
                          "Comment=Desktop client for Matrix\n"
                          "Exec=%2 -p %3 %u\n"
                          "Icon=%4\n"
                          "Categories=Network;InstantMessaging;Qt;\n"
                          "Terminal=false\n"
                          "X-GNOME-UsesNotifications=true\n")
      .arg(normalizedProfile,
           desktopExecArg(executablePath),
           desktopExecArg(normalizedProfile),
           QString::fromLatin1(komai::desktop_icon_name));
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
profileDirectory(QStringView profileId)
{
    return root() + QStringLiteral("/profiles/") + normalizedProfileId(profileId);
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

namespace desktop {

bool
supportsProfileDesktopEntries()
{
#if defined(Q_OS_LINUX)
    const auto envTag = keyring_environment::tag();
    return envTag != QLatin1String("flatpak") && envTag != QLatin1String("snap");
#else
    return false;
#endif
}

QString
profileDesktopEntryId(QStringView profileId)
{
    return QStringLiteral("%1.profile.%2")
      .arg(QString::fromLatin1(komai::desktop_id), normalizedProfileId(profileId));
}

QString
applicationsDirectory()
{
    const auto applicationsDir =
      QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation);
    if (!applicationsDir.isEmpty())
        return applicationsDir;

    const auto dataRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    if (!dataRoot.isEmpty())
        return dataRoot + QStringLiteral("/applications");

    return {};
}

QString
profileDesktopEntryFile(QStringView profileId)
{
    const auto applicationsDir = applicationsDirectory();
    if (applicationsDir.isEmpty())
        return {};

    return applicationsDir + QStringLiteral("/") + profileDesktopEntryId(profileId) +
           QStringLiteral(".desktop");
}

QString
findInstalledProfileDesktopEntry(QStringView profileId)
{
    const auto fileName = profileDesktopEntryId(profileId) + QStringLiteral(".desktop");
    for (const auto &applicationsDir : desktopApplicationsSearchDirectories()) {
        const auto filePath = applicationsDir + QStringLiteral("/") + fileName;
        if (QFileInfo::exists(filePath))
            return filePath;
    }

    return {};
}

bool
ensureProfileDesktopEntry(QStringView profileId, QStringView executablePath, QString *errorOut)
{
    if (!supportsProfileDesktopEntries()) {
        if (errorOut)
            *errorOut = QStringLiteral("generated profile desktop entries are not supported here");
        return false;
    }

    if (executablePath.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("current executable path is empty");
        return false;
    }

    const auto applicationsDir = applicationsDirectory();
    if (applicationsDir.isEmpty()) {
        if (errorOut)
            *errorOut = QStringLiteral("applications directory is unavailable");
        return false;
    }

    if (!QDir().mkpath(applicationsDir)) {
        if (errorOut)
            *errorOut =
              QStringLiteral("failed to create applications directory: %1").arg(applicationsDir);
        return false;
    }

    const auto filePath = profileDesktopEntryFile(profileId);
    const auto contents = profileDesktopEntryContents(profileId, executablePath);

    QFile currentFile(filePath);
    if (currentFile.open(QIODevice::ReadOnly | QIODevice::Text) &&
        QString::fromUtf8(currentFile.readAll()) == contents) {
        return true;
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorOut)
            *errorOut =
              QStringLiteral("failed to open desktop entry for writing: %1").arg(filePath);
        return false;
    }

    if (file.write(contents.toUtf8()) < 0) {
        if (errorOut)
            *errorOut = QStringLiteral("failed to write desktop entry: %1").arg(filePath);
        return false;
    }

    if (!file.commit()) {
        if (errorOut)
            *errorOut = QStringLiteral("failed to commit desktop entry: %1").arg(filePath);
        return false;
    }

    return true;
}

bool
removeProfileDesktopEntry(QStringView profileId, QString *errorOut)
{
    if (!supportsProfileDesktopEntries())
        return true;

    const auto filePath = profileDesktopEntryFile(profileId);
    if (filePath.isEmpty())
        return true;

    const QFileInfo info(filePath);
    if (!info.exists())
        return true;

    if (QFile::remove(filePath))
        return true;

    if (errorOut)
        *errorOut = QStringLiteral("failed to remove desktop entry: %1").arg(filePath);
    return false;
}

} // namespace desktop

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
