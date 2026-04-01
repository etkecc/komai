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
