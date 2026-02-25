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
UserSettings::setUiAvatarsCircular(bool s)
{
    if (uiAvatarsCircular_ == s)
        return;
    uiAvatarsCircular_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiAvatarsCircular, s);
    emit uiAvatarsCircularChanged(s);
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

void
UserSettings::setUiMotionAnimationsEnabled(bool state)
{
    if (state == uiMotionAnimationsEnabled_)
        return;
    uiMotionAnimationsEnabled_ = state;
    (void)coreStore_.set(settings::core::SettingId::UiMotionAnimationsEnabled, state);
    emit uiMotionAnimationsEnabledChanged(state);
    save();

    // Also toggle other motion related settings
    if (!uiMotionAnimationsEnabled_) {
        setTimelineMediaEffectsEnabled(false);
        setTimelineMediaAnimateOnHover(true);
    }
}

void
UserSettings::setPrivacyWindowFocusBlurEnabled(bool s)
{
    if (privacyWindowFocusBlurEnabled_ == s)
        return;
    privacyWindowFocusBlurEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyWindowFocusBlurEnabled, s);
    emit privacyWindowFocusBlurEnabledChanged(s);
    save();
}
void
UserSettings::setPrivacyWindowFocusBlurDelaySeconds(int s)
{
    if (privacyWindowFocusBlurDelaySeconds_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds,
                      s,
                      SettingKey::PrivacyWindowFocusBlurDelaySeconds))
        return;
    privacyWindowFocusBlurDelaySeconds_ = s;
    emit privacyWindowFocusBlurDelaySecondsChanged(s);
    save();
}
