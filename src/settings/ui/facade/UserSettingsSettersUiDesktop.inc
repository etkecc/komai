void
UserSettings::setNotificationsAccountEnabled(bool s)
{
    if (!hasActiveSession()) {
        nhlog::ui()->debug(
          "Ignoring notifications account toggle without an active session");
        return;
    }

    if (!notificationsAccountHandleProvider_ || !notificationsAccountSetFn_) {
        nhlog::ui()->warn("Ignoring account-wide notification toggle change because no "
                          "matrix-sdk notification runtime hooks are installed");
        return;
    }

    const auto handleId = notificationsAccountHandleProvider_();
    if (handleId == 0) {
        nhlog::ui()->warn(
          "Ignoring account-wide notification toggle change because no matrix-sdk runtime "
          "handle is active");
        return;
    }

    if (notificationsAccountEnabledLoaded_ && notificationsAccountEnabled_ == s &&
        !notificationsAccountEnabledInFlight_)
        return;

    const bool previousValue = notificationsAccountEnabled_;

    notificationsAccountEnabled_         = s;
    notificationsAccountEnabledLoaded_   = true;
    notificationsAccountEnabledInFlight_ = true;
    const auto requestId                 = ++notificationsAccountEnabledRequestId_;
    const auto setFn                     = notificationsAccountSetFn_;

    if (previousValue != s)
        emit notificationsAccountEnabledChanged(s);

    komai::qt_worker_task::runQueued(
      this,
      [setFn, handleId, s]() {
          QString error;
          const bool ok = setFn(handleId, s, &error);
          return std::make_pair(ok, error);
      },
      [handleId, requestId, previousValue](
        UserSettings *settings, const std::pair<bool, QString> &result) {
          if (requestId != settings->notificationsAccountEnabledRequestId_)
              return;

          settings->notificationsAccountEnabledInFlight_ = false;

          const auto activeHandleId = settings->notificationsAccountHandleProvider_
                                        ? settings->notificationsAccountHandleProvider_()
                                        : 0;
          if (!settings->hasActiveSession() || activeHandleId != handleId)
              return;

          const auto &[ok, error] = result;
          if (ok)
              return;

          settings->notificationsAccountEnabledLoaded_ = false;
          nhlog::ui()->warn(
            "Failed to update account notification toggle through the matrix-sdk runtime: {}",
            error.toStdString());

          if (settings->notificationsAccountEnabled_ == previousValue)
              return;

          settings->notificationsAccountEnabled_ = previousValue;
          emit settings->notificationsAccountEnabledChanged(previousValue);
      });
}

void
UserSettings::setDesktopNotificationsEnabled(bool s)
{
    if (desktopNotificationsEnabled_ == s)
        return;
    desktopNotificationsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::DesktopNotificationsEnabled, s);
    emit desktopNotificationsEnabledChanged(s);
    save();
}

void
UserSettings::setDesktopNotificationsAttentionOnIncoming(bool s)
{
    if (desktopNotificationsAttentionOnIncoming_ == s)
        return;
    desktopNotificationsAttentionOnIncoming_ = s;
    (void)coreStore_.set(settings::core::SettingId::DesktopNotificationsAttentionOnIncoming, s);
    emit desktopNotificationsAttentionOnIncomingChanged(s);
    save();
}

void
UserSettings::setDesktopNotificationsMessageContentPolicy(NotificationMessageContentPolicy s)
{
    if (desktopNotificationsMessageContentPolicy_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::DesktopNotificationsMessageContentPolicy,
                      static_cast<int>(s),
                      "desktop.notifications.message_content_policy"))
        return;
    desktopNotificationsMessageContentPolicy_ = s;
    emit desktopNotificationsMessageContentPolicyChanged(s);
    save();
}

void
UserSettings::setDesktopWindowFocusBlurEnabled(bool s)
{
    if (desktopWindowFocusBlurEnabled_ == s)
        return;
    desktopWindowFocusBlurEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::DesktopWindowFocusBlurEnabled, s);
    emit desktopWindowFocusBlurEnabledChanged(s);
    save();
}

void
UserSettings::setDesktopWindowFocusBlurDelaySeconds(int s)
{
    if (desktopWindowFocusBlurDelaySeconds_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::DesktopWindowFocusBlurDelaySeconds,
                      s,
                      SettingKey::DesktopWindowFocusBlurDelaySeconds))
        return;
    desktopWindowFocusBlurDelaySeconds_ = s;
    emit desktopWindowFocusBlurDelaySecondsChanged(s);
    save();
}
