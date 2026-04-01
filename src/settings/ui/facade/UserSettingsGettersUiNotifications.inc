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
    if (!hasActiveSession() || !notificationsAccountHandleProvider_ || !notificationsAccountFetchFn_ ||
        !notificationsAccountSetFn_)
        return false;

    return notificationsAccountHandleProvider_() != 0;
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

    if (!notificationsAccountHandleProvider_ || !notificationsAccountFetchFn_)
        return;

    const auto handleId = notificationsAccountHandleProvider_();
    if (handleId == 0)
        return;

    if (notificationsAccountEnabledLoaded_ || notificationsAccountEnabledInFlight_)
        return;

    notificationsAccountEnabledInFlight_ = true;
    const auto requestId                 = ++notificationsAccountEnabledRequestId_;
    const auto fetchFn                   = notificationsAccountFetchFn_;

    komai::qt_worker_task::runQueued(
      this,
      [fetchFn, handleId]() {
          QString error;
          return std::make_pair(fetchFn(handleId, &error), error);
      },
      [handleId, requestId](
        UserSettings *settings,
        const std::pair<std::optional<bool>, QString> &result) {
          if (requestId != settings->notificationsAccountEnabledRequestId_)
              return;

          settings->notificationsAccountEnabledInFlight_ = false;

          const auto activeHandleId = settings->notificationsAccountHandleProvider_
                                        ? settings->notificationsAccountHandleProvider_()
                                        : 0;
          if (!settings->hasActiveSession() || activeHandleId != handleId)
              return;

          const auto &[enabled, error] = result;
          if (!enabled.has_value()) {
              settings->notificationsAccountEnabledLoaded_ = false;
              nhlog::ui()->warn(
                "Failed to fetch account notification toggle from the matrix-sdk runtime: {}",
                error.toStdString());
              return;
          }

          settings->notificationsAccountEnabledLoaded_ = true;

          if (settings->notificationsAccountEnabled_ == *enabled)
              return;

          settings->notificationsAccountEnabled_ = *enabled;
          emit settings->notificationsAccountEnabledChanged(*enabled);
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
