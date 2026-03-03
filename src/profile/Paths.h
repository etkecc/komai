// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QSize>
#include <QString>
#include <QStringList>
#include <QStringView>

namespace app_paths {

QString
normalizedProfileId(QStringView profileId);
QString
encodedIdComponent(QStringView value);

namespace config {
QString
profileConfigFile(QStringView profileId);
QString
profileStateFile(QStringView profileId);
QString
profileSessionFile(QStringView profileId);
QString
profileSecretsFile(QStringView profileId);
} // namespace config

namespace data {
QString
dbRoot(QStringView profileId);
QString
databaseDirectory(QStringView userId, QStringView profileId);
QString
userThemesDirectory();
QStringList
themeSearchDirectories();
} // namespace data

namespace cache {
QString
mediaDirectory(QStringView profileId);
QString
mediaMediaDirectory(QStringView profileId);
QString
mediaFileForMxc(QStringView profileId, QStringView mxcId, QStringView suffix);
QString
mediaMediaFileForMxc(QStringView profileId, QStringView mxcId, QStringView suffix);
QString
mediaThumbnailFileForMxc(QStringView profileId,
                         QStringView mxcId,
                         const QSize &requestedSize,
                         bool crop,
                         double radius);
QString
roomNotificationAvatarFile(QStringView profileId, QStringView roomId);
QString
logFile(QStringView profileId);
QString
altSvcCacheFile(QStringView profileId);
} // namespace cache

} // namespace app_paths
