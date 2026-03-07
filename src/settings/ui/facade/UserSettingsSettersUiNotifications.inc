void
UserSettings::setNotificationsAccountEnabled(bool s)
{
    if (!hasActiveSession()) {
        nhlog::ui()->debug(
          "Ignoring notifications account toggle without an active session");
        return;
    }

    if (notificationsAccountEnabledInFlight_)
        return;

    const bool oldValue = notificationsAccountEnabled_;
    if (oldValue == s && notificationsAccountEnabledLoaded_)
        return;

    notificationsAccountEnabled_         = s;
    notificationsAccountEnabledInFlight_ = true;
    emit notificationsAccountEnabledChanged(s);
    emit sessionAuthStateChanged();

    http::client()->put_pushrules_enabled(
      "global",
      "override",
      ".m.rule.master",
      !s,
      [this, oldValue](mtx::http::RequestErr err) {
          notificationsAccountEnabledInFlight_ = false;
          emit sessionAuthStateChanged();

          if (err) {
              nhlog::net()->error("failed to set '.m.rule.master' pushrule: {} {}",
                                  static_cast<int>(err->status_code),
                                  err->matrix_error.error);

              if (notificationsAccountEnabled_ != oldValue) {
                  notificationsAccountEnabled_ = oldValue;
                  emit notificationsAccountEnabledChanged(oldValue);
              }
              return;
          }

          notificationsAccountEnabledLoaded_ = true;
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
