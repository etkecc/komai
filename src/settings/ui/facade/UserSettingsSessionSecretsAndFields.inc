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
