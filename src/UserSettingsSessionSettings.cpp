// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QString>

#include "Logging.h"
#include "Paths.h"
#include "UserSettingsPage.h"
#include "settings/SettingsController.h"
#include "settings/SettingsStorage.h"

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

void
UserSettings::clearAuth()
{
    settings::SettingsController controller;
    controller.clearAuth(*this);
}

bool
UserSettings::hasPersistedSessionIdentity() const
{
    return hasSessionIdentity(sessionSnapshot());
}

bool
UserSettings::hasActiveSession() const
{
    return hasCompleteSessionAuth(sessionSnapshot());
}

UserSettings::SessionSnapshot
UserSettings::sessionSnapshot() const
{
    return SessionSnapshot{.userId      = userId_,
                           .accessToken = accessToken_,
                           .deviceId    = deviceId_,
                           .homeserver  = homeserver_};
}

bool
UserSettings::persistSessionSnapshot(const SessionSnapshot &snapshot)
{
    nhlog::ui()->info("Persisting session snapshot for profile '{}' "
                      "(has_user_id={}, has_access_token={}, has_device_id={}, has_homeserver={})",
                      app_paths::normalizedProfileId(profile_).toStdString(),
                      hasSessionValue(snapshot.userId),
                      hasSessionValue(snapshot.accessToken),
                      hasSessionValue(snapshot.deviceId),
                      hasSessionValue(snapshot.homeserver));

    if (!hasCompleteSessionAuth(snapshot)) {
        nhlog::ui()->warn(
          "Refusing to persist incomplete session snapshot "
          "(has_user_id={}, has_access_token={}, has_device_id={}, has_homeserver={})",
          hasSessionValue(snapshot.userId),
          hasSessionValue(snapshot.accessToken),
          hasSessionValue(snapshot.deviceId),
          hasSessionValue(snapshot.homeserver));
        return false;
    }

    bool changed = false;

    auto applyField = [this, &changed](QString &field, const QString &value, auto signal) {
        if (field == value)
            return;

        field = value;
        emit(this->*signal)(value);
        changed = true;
    };

    applyField(userId_, snapshot.userId, &UserSettings::userIdChanged);
    applyField(accessToken_, snapshot.accessToken, &UserSettings::accessTokenChanged);
    applyField(deviceId_, snapshot.deviceId, &UserSettings::deviceIdChanged);
    applyField(homeserver_, snapshot.homeserver, &UserSettings::homeserverChanged);

    if (!changed)
        nhlog::ui()->debug("Persisted session snapshot unchanged; rewriting session/auth storage");
    else
        nhlog::ui()->info("Persisted session snapshot fields to storage");

    // Always write on explicit auth persist requests; in-memory equality does not
    // guarantee that session.yml / secrets.yml / secure backend values are present.
    save();

    return true;
}

QString
UserSettings::secret(const QString &name) const
{
    return secrets_.value(name, QString());
}

void
UserSettings::setSecret(const QString &name, const QString &value)
{
    if (value.isEmpty()) {
        removeSecret(name);
        return;
    }

    secrets_[name] = value;
    save();
}

void
UserSettings::removeSecret(const QString &name)
{
    secrets_.remove(name);
    save();
}

void
UserSettings::setProfile(QString profile)
{
    // always set this to allow setting this when loading and it is overwritten on the cli
    profile_         = profile;
    profileDirPath_  = profileDirPath(profile_);
    configFilePath_  = configFilePathForProfile(profile_);
    stateFilePath_   = stateFilePathForProfile(profile_);
    sessionFilePath_ = sessionFilePathForProfile(profile_);
    secretsFilePath_ = secretsFilePathForProfile(profile_);
    emit profileChanged(profile_);
    save();
}

void
UserSettings::setUserId(QString s)
{
    if (s == userId_)
        return;

    userId_ = s;
    emit userIdChanged(s);
    save();
}

void
UserSettings::setAccessToken(QString s)
{
    if (s == accessToken_)
        return;

    accessToken_ = s;
    emit accessTokenChanged(s);
    save();
}

void
UserSettings::setDeviceId(QString s)
{
    if (s == deviceId_)
        return;

    deviceId_ = s;
    emit deviceIdChanged(s);
    save();
}

void
UserSettings::setHomeserver(QString s)
{
    if (s == homeserver_)
        return;

    homeserver_ = s;
    emit homeserverChanged(s);
    save();
}
