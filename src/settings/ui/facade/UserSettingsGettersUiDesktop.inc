bool
UserSettings::desktopSystemTrayEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::DesktopSystemTrayEnabled);
        value.has_value())
        return *value;
    return desktopSystemTrayEnabled_;
}

bool
UserSettings::desktopSystemTrayAutostart() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::DesktopSystemTrayAutostart);
        value.has_value())
        return *value;
    return desktopSystemTrayAutostart_;
}

UserSettings::NotificationMessageContentPolicy
UserSettings::desktopNotificationsMessageContentPolicy() const
{
    if (const auto value = coreStore_.valueAs<int>(
          settings::core::SettingId::DesktopNotificationsMessageContentPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::NotificationMessageContentPolicy::Never) &&
        *value <=
          static_cast<int>(UserSettings::NotificationMessageContentPolicy::WheneverAvailable))
        return static_cast<UserSettings::NotificationMessageContentPolicy>(*value);
    return desktopNotificationsMessageContentPolicy_;
}

bool
UserSettings::notificationsAccountEnabled() const
{
    return notificationsAccountEnabled_;
}

bool
UserSettings::desktopNotificationsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::DesktopNotificationsEnabled);
        value.has_value())
        return *value;
    return desktopNotificationsEnabled_;
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
UserSettings::desktopNotificationsAttentionOnIncoming() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::DesktopNotificationsAttentionOnIncoming);
        value.has_value())
        return *value;
    return desktopNotificationsAttentionOnIncoming_;
}

bool
UserSettings::desktopAttentionWindowTitleEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::DesktopAttentionWindowTitleEnabled);
        value.has_value())
        return *value;
    return desktopAttentionWindowTitleEnabled_;
}

bool
UserSettings::desktopAttentionAppBadgeEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::DesktopAttentionAppBadgeEnabled);
        value.has_value())
        return *value;
    return desktopAttentionAppBadgeEnabled_;
}

bool
UserSettings::hasNotifications() const
{
    if (hasActiveSession() && !notificationsAccountEnabled_)
        return false;

    return desktopNotificationsEnabled() || desktopNotificationsAttentionOnIncoming();
}

bool
UserSettings::desktopWindowFocusBlurEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::DesktopWindowFocusBlurEnabled);
        value.has_value())
        return *value;
    return desktopWindowFocusBlurEnabled_;
}

int
UserSettings::desktopWindowFocusBlurDelaySeconds() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::DesktopWindowFocusBlurDelaySeconds);
        value.has_value())
        return *value;
    return desktopWindowFocusBlurDelaySeconds_;
}
