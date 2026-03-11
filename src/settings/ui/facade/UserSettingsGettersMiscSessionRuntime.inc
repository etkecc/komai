QString
UserSettings::profile() const
{
    return profile_;
}
QString
UserSettings::userId() const
{
    return userId_;
}
QString
UserSettings::accessToken() const
{
    return accessToken_;
}
QString
UserSettings::deviceId() const
{
    return deviceId_;
}
QString
UserSettings::currentFilterId() const
{
    return currentFilterId_;
}
QString
UserSettings::currentRoomId() const
{
    return currentRoomId_;
}
QString
UserSettings::homeserver() const
{
    return homeserver_;
}
QStringList
UserSettings::globalExcludes() const
{
    return globalExcludes_;
}
QStringList
UserSettings::badgesHiddenFilters() const
{
    return badgesHiddenFilters_;
}
QStringList
UserSettings::hiddenPins() const
{
    return hiddenPins_;
}
QStringList
UserSettings::hiddenWidgets() const
{
    return hiddenWidgets_;
}
const QMap<QString, QString> &
UserSettings::composerDraftsByRoom() const
{
    return composerDraftsByRoom_;
}

QString
UserSettings::composerDraftForRoom(const QString &roomId) const
{
    return composerDraftsByRoom_.value(roomId);
}

bool
UserSettings::hasComposerDraftForRoom(const QString &roomId) const
{
    const auto it = composerDraftsByRoom_.constFind(roomId);
    return it != composerDraftsByRoom_.constEnd() && !it.value().trimmed().isEmpty();
}

QStringList
UserSettings::collapsedSpaces() const
{
    return collapsedSpaces_;
}
int
UserSettings::windowWidth() const
{
    return windowWidth_;
}
int
UserSettings::windowHeight() const
{
    return windowHeight_;
}
qulonglong
UserSettings::dbMaxSizeBytes() const
{
    return dbMaxSizeBytes_;
}
uint
UserSettings::dbMaxStores() const
{
    return dbMaxStores_;
}

// Internal helper: secrets provider is configured via `secrets.provider` in config.yml.
bool
UserSettings::usesFileSecretsProvider() const
{
    return usesFileSecretsProvider_;
}

bool
UserSettings::secretsProviderFallbackWarningVisible() const
{
    return secretsProviderFallbackWarningVisible_;
}
