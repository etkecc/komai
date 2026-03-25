UserSettings::NotificationMessageContentPolicy
UserSettings::notificationsMessageContentPolicy() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NotificationsMessageContentPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::NotificationMessageContentPolicy::Never) &&
        *value <=
          static_cast<int>(UserSettings::NotificationMessageContentPolicy::WheneverAvailable))
        return static_cast<UserSettings::NotificationMessageContentPolicy>(*value);
    return notificationsMessageContentPolicy_;
}

bool
UserSettings::notificationsAccountEnabled() const
{
    return notificationsAccountEnabled_;
}

bool
UserSettings::notificationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NotificationsEnabled);
        value.has_value())
        return *value;
    return notificationsEnabled_;
}

bool
UserSettings::notificationsAccountToggleEnabled() const
{
    return false;
}

void
UserSettings::refreshNotificationsAccountEnabled()
{
    if (!hasActiveSession()) {
        notificationsAccountEnabledLoaded_   = false;
        notificationsAccountEnabledInFlight_ = false;

        if (!notificationsAccountEnabled_) {
            notificationsAccountEnabled_ = true;
            emit notificationsAccountEnabledChanged(true);
        }

        return;
    }

    if (notificationsAccountEnabledLoaded_ || notificationsAccountEnabledInFlight_)
        return;

    notificationsAccountEnabledInFlight_ = false;
    notificationsAccountEnabledLoaded_   = true;

    if (!notificationsAccountEnabled_) {
        notificationsAccountEnabled_ = true;
        emit notificationsAccountEnabledChanged(true);
    }
}

bool
UserSettings::notificationsAttentionOnIncoming() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NotificationsAttentionOnIncoming);
        value.has_value())
        return *value;
    return notificationsAttentionOnIncoming_;
}

bool
UserSettings::hasNotifications() const
{
    if (hasActiveSession() && !notificationsAccountEnabled_)
        return false;

    return notificationsEnabled() || notificationsAttentionOnIncoming();
}
