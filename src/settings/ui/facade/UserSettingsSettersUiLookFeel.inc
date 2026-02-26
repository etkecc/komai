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
UserSettings::setUiLayoutCompactMode(bool s)
{
    if (uiLayoutCompactMode_ == s)
        return;
    uiLayoutCompactMode_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiLayoutCompactMode, s);
    emit uiLayoutCompactModeChanged(s);
    save();
}
