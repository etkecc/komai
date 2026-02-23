// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QString>

#include "Logging.h"
#include "Paths.h"
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

void
UserSettings::clearAuth()
{
    settings::SettingsController controller;
    controller.clearAuth(*this);
    setPersistenceScopeReadyForAuth(false);
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

void
UserSettings::setSessionSnapshot(const SessionSnapshot &snapshot)
{
    const auto applyField = [this](QString &field, const QString &value, auto signal) {
        if (field == value)
            return;

        field = value;
        emit(this->*signal)(value);
    };

    applyField(userId_, snapshot.userId, &UserSettings::userIdChanged);
    applyField(deviceId_, snapshot.deviceId, &UserSettings::deviceIdChanged);
    applyField(homeserver_, snapshot.homeserver, &UserSettings::homeserverChanged);
}

void
UserSettings::setUsesFileSecretsProvider(bool usesFileSecretsProvider)
{
    usesFileSecretsProvider_ = usesFileSecretsProvider;
}

bool
UserSettings::hasResolvedProfilePaths() const
{
    return !profileDirPath_.isEmpty();
}

const QString &
UserSettings::profileId() const
{
    return profile_;
}

const QString &
UserSettings::profileDirPath() const
{
    return profileDirPath_;
}

const QString &
UserSettings::configFilePath() const
{
    return configFilePath_;
}

const QString &
UserSettings::stateFilePath() const
{
    return stateFilePath_;
}

const QString &
UserSettings::sessionFilePath() const
{
    return sessionFilePath_;
}

const QString &
UserSettings::secretsFilePath() const
{
    return secretsFilePath_;
}

const QMap<QString, QString> &
UserSettings::secretsMap() const
{
    return secrets_;
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
    setPersistenceScopeReadyForAuth(true);
    save();

    return true;
}

void
UserSettings::applyLoadedSecrets(const QString &accessToken, const QMap<QString, QString> &secrets)
{
    accessToken_ = accessToken;
    secrets_     = secrets;
}

void
UserSettings::clearAuthInMemory()
{
    accessToken_ = QString();
    homeserver_  = QString();
    userId_      = QString();
    deviceId_    = QString();
    secrets_.clear();
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
UserSettings::applyProfilePathState(const QString &profile)
{
    profile_         = profile;
    profileDirPath_  = settings::storage::profileDirPath(profile_);
    configFilePath_  = configFilePathForProfile(profile_);
    stateFilePath_   = stateFilePathForProfile(profile_);
    sessionFilePath_ = sessionFilePathForProfile(profile_);
    secretsFilePath_ = secretsFilePathForProfile(profile_);
}

void
UserSettings::setProfile(QString profile)
{
    // always set this to allow setting this when loading and it is overwritten on the cli
    applyProfilePathState(profile);
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
