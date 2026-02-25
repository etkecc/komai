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
