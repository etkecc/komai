QString
notificationsMessageContentPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.desktopNotificationsMessageContentPolicy());
}

void
applyNotificationMessageContentPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setDesktopNotificationsMessageContentPolicy(
      notificationsMessageContentPolicyFromStorage(
        rawToken, UserSettings::NotificationMessageContentPolicy::WheneverAvailable));
}
