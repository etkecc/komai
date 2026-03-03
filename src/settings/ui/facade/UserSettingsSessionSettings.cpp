// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QString>

#include "Logging.h"
#include "profile/Paths.h"
#include "settings/SettingsController.h"
#include "settings/SettingsStorage.h"
#include "settings/ui/facade/UserSettingsPage.h"

namespace {

using settings::storage::configFilePathForProfile;
using settings::storage::profileDirPath;
using settings::storage::secretsFilePathForProfile;
using settings::storage::sessionFilePathForProfile;
using settings::storage::stateFilePathForProfile;

bool
hasSessionValue(const QString &value)
{
    return !value.trimmed().isEmpty();
}

bool
hasSessionIdentity(const UserSettings::SessionSnapshot &snapshot)
{
    return hasSessionValue(snapshot.userId) && hasSessionValue(snapshot.deviceId) &&
           hasSessionValue(snapshot.homeserver);
}

bool
hasCompleteSessionAuth(const UserSettings::SessionSnapshot &snapshot)
{
    return hasSessionIdentity(snapshot) && hasSessionValue(snapshot.accessToken);
}

} // namespace

#include "UserSettingsSessionAuth.inc"
#include "UserSettingsSessionProfileState.inc"
#include "UserSettingsSessionSecretsAndFields.inc"
