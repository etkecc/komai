QString
presenceToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.networkPresenceStatusPolicy());
}

void
applyPresenceFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNetworkPresenceStatusPolicy(
      presenceFromStorage(rawToken, UserSettings::Presence::AutomaticPresence));
}
