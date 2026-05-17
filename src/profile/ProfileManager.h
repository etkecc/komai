// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <QStringView>
#include <QVector>

#include <optional>

namespace profile_manager {

struct ProfileSummary
{
    QString id;
    QString userId;
    QString homeserver;
    QString themeSlug;
    QString secretsProvider;
    QColor accentColor;
    QColor windowColor;
    QColor darkColor;
    QColor textColor;
    QColor brightTextColor;
    bool isDefault{false};
    bool isCurrent{false};
};

QStringList
listProfileIds();
QVector<ProfileSummary>
listProfiles(QStringView currentProfile = {});

bool
shouldShowStartupSelector(bool profileArgumentProvided, QStringView selectedProfile);

std::optional<QString>
validateNewProfileId(QStringView profileId);

bool
ensureProfileDesktopLauncher(QStringView profileId, QString *errorOut = nullptr);
bool
launchProfileDetached(QStringView profileId, QString *errorOut = nullptr);
bool
launchStartupSelectorDetached(QString *errorOut = nullptr);

//! Stash a `matrix:` URI captured at startup (or via single-instance IPC) so
//! the next `launchProfileDetached()` call forwards it to the spawned
//! process. The slot is cleared on read. Used by the profile selector to keep
//! a URI received via the OS URL handler from being dropped when the user
//! picks a profile.
void
setPendingForwardedMatrixUri(QString uri);

bool
deleteProfile(QStringView profileId,
              QStringView currentProfileId,
              QString *errorOut          = nullptr,
              bool protectCurrentProfile = true);

} // namespace profile_manager
