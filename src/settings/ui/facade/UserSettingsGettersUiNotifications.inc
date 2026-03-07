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
    return hasActiveSession() && !notificationsAccountEnabledInFlight_;
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

    notificationsAccountEnabledInFlight_ = true;
    emit sessionAuthStateChanged();

    http::client()->get_pushrules_enabled(
      "global",
      "override",
      ".m.rule.master",
      [this](const mtx::pushrules::Enabled &ruleEnabled, mtx::http::RequestErr err) {
          notificationsAccountEnabledInFlight_ = false;
          emit sessionAuthStateChanged();

          if (err) {
              nhlog::net()->warn("failed to fetch '.m.rule.master' pushrule: {} {}",
                                 static_cast<int>(err->status_code),
                                 err->matrix_error.error);
              return;
          }

          notificationsAccountEnabledLoaded_ = true;
          const bool enabled                 = !ruleEnabled.enabled;
          if (notificationsAccountEnabled_ == enabled)
              return;

          notificationsAccountEnabled_ = enabled;
          emit notificationsAccountEnabledChanged(enabled);
      });
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
