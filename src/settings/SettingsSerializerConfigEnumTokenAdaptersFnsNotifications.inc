QString
notificationsMessageContentPolicyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.notificationsMessageContentPolicy());
}

void
applyNotificationMessageContentPolicyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setNotificationsMessageContentPolicy(notificationsMessageContentPolicyFromStorage(
      rawToken, UserSettings::NotificationMessageContentPolicy::WheneverAvailable));
}
