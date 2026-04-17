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
}

void
UserSettings::setUiLayoutDensity(Density s)
{
    if (uiLayoutDensity_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::UiLayoutDensity,
                      static_cast<int>(s),
                      SettingKey::UiLayoutDensity))
        return;
    uiLayoutDensity_ = s;
    emit uiLayoutDensityChanged(s);
    save();
}
