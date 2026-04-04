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
profileDirectory(QStringView profileId);
QString
userThemesDirectory();
QStringList
themeSearchDirectories();
} // namespace data

namespace desktop {

/// Whether this runtime can manage per-profile desktop entries for launcher/badge isolation.
bool
supportsProfileDesktopEntries();

/// Returns a valid desktop-entry ID for a profile-scoped app identity.
QString
profileDesktopEntryId(QStringView profileId);

/// Returns the user-local applications directory used for generated desktop entries.
QString
applicationsDirectory();

/// Returns the full user-local desktop entry path for a profile.
QString
profileDesktopEntryFile(QStringView profileId);

/// Returns the first matching existing desktop entry path for a profile, or empty if none exists.
QString
findInstalledProfileDesktopEntry(QStringView profileId);

/// Creates or updates the generated user-local desktop entry for a profile.
bool
ensureProfileDesktopEntry(QStringView profileId,
                          QStringView executablePath,
                          QString *errorOut = nullptr);

/// Removes the generated user-local desktop entry for a profile.
bool
removeProfileDesktopEntry(QStringView profileId, QString *errorOut = nullptr);

} // namespace desktop

namespace cache {

/// Maximum age (in days since last access) before cached media files
/// are eligible for automatic removal by the hourly purge timer.
inline constexpr int mediaPurgeAgeDays = 14;

QString
root();

/// Profile-specific cache directory root.
QString
profileDirectory(QStringView profileId);

/// Root of all cached media for a profile (for stats, total purge).
QString
mediaRoot(QStringView profileId);

/// Per-room media cache directory.
QString
roomMediaDirectory(QStringView profileId, QStringView roomId);

/// Shared (cross-room) media cache directory (avatars, stickers, etc.).
QString
sharedMediaDirectory(QStringView profileId);

/// Cache path for a downloaded media file.
/// If roomId is non-empty the file is placed under the room's directory,
/// otherwise under the shared directory.
QString
mediaFileForMxc(QStringView profileId,
                QStringView mxcId,
                QStringView suffix,
                QStringView roomId = {});

/// Cache path for a full download when the file suffix is unknown.
QString
mediaFullFileForMxc(QStringView profileId, QStringView mxcId, QStringView roomId = {});

/// Cache path for a thumbnail.
QString
mediaThumbnailFileForMxc(QStringView profileId,
                         QStringView mxcId,
                         const QSize &requestedSize,
                         bool crop,
                         double radius,
                         QStringView roomId = {});

QString
roomNotificationAvatarFile(QStringView profileId, QStringView roomId);

QString
logDirectory(QStringView profileId);
QString
logFile(QStringView profileId);

QString
httpCacheDirectory(QStringView profileId);
QString
altSvcCacheFile(QStringView profileId);
} // namespace cache

} // namespace app_paths
