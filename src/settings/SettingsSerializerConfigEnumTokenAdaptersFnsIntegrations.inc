QString
dbusAccessToStorageFromSettings(const UserSettings &settings)
{
    return dbusAccessToStorage(settings.integrationsDbusApiAccess());
}

void
applyDbusAccessFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setIntegrationsDbusApiAccess(
      dbusAccessFromStorage(rawToken, IntegrationsDbusAccessNone));
}
