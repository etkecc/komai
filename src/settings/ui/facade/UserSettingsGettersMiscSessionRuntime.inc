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

QStringList
UserSettings::hiddenTimelineEventTypes() const
{
    return hiddenTimelineEventTypes_;
}

const QMap<QString, QStringList> &
UserSettings::hiddenTimelineEventTypesByRoom() const
{
    return hiddenTimelineEventTypesByRoom_;
}

QStringList
UserSettings::hiddenTimelineEventTypesForRoom(const QString &roomId) const
{
    const auto trimmedRoomId = roomId.trimmed();
    if (trimmedRoomId.isEmpty())
        return hiddenTimelineEventTypes_;

    const auto it = hiddenTimelineEventTypesByRoom_.constFind(trimmedRoomId);
    return it != hiddenTimelineEventTypesByRoom_.constEnd() ? it.value()
                                                            : hiddenTimelineEventTypes_;
}

bool
UserSettings::isTimelineEventHiddenInRoom(int eventType, const QString &roomId) const
{
    const auto key =
      qml_mtx_events::localTimelineEventTypeKey(static_cast<qml_mtx_events::EventType>(eventType));
    if (key.isEmpty())
        return false;

    return hiddenTimelineEventTypesForRoom(roomId).contains(key);
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
QStringList
UserSettings::hiddenSpaces() const
{
    return hiddenSpaces_;
}
QString
UserSettings::donationStatus() const
{
    return donationStatus_;
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
