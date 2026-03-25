void
UserSettings::setNotificationsAccountEnabled(bool s)
{
    if (!hasActiveSession()) {
        nhlog::ui()->debug(
          "Ignoring notifications account toggle without an active session");
        return;
    }

    nhlog::ui()->warn(
      "Ignoring account-wide notification toggle change; push-rule sync is not migrated to "
      "matrix-sdk yet (requested={})",
      s);
}

void
UserSettings::setNotificationsEnabled(bool s)
{
    if (notificationsEnabled_ == s)
        return;
    notificationsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::NotificationsEnabled, s);
    emit notificationsEnabledChanged(s);
    save();
}

void
UserSettings::setNotificationsAttentionOnIncoming(bool s)
{
    if (notificationsAttentionOnIncoming_ == s)
        return;
    notificationsAttentionOnIncoming_ = s;
    (void)coreStore_.set(settings::core::SettingId::NotificationsAttentionOnIncoming, s);
    emit notificationsAttentionOnIncomingChanged(s);
    save();
}

void
UserSettings::setNotificationsMessageContentPolicy(NotificationMessageContentPolicy s)
{
    if (notificationsMessageContentPolicy_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NotificationsMessageContentPolicy,
                      static_cast<int>(s),
                      "notifications.message_content_policy"))
        return;
    notificationsMessageContentPolicy_ = s;
    emit notificationsMessageContentPolicyChanged(s);
    save();
}
