void
UserSettings::setDesktopSystemTrayEnabled(bool s)
{
    if (desktopSystemTrayEnabled_ == s)
        return;
    desktopSystemTrayEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::DesktopSystemTrayEnabled, s);
    emit desktopSystemTrayEnabledChanged(s);
    save();
}

void
UserSettings::setDesktopSystemTrayAutostart(bool s)
{
    if (desktopSystemTrayAutostart_ == s)
        return;
    desktopSystemTrayAutostart_ = s;
    (void)coreStore_.set(settings::core::SettingId::DesktopSystemTrayAutostart, s);
    emit desktopSystemTrayAutostartChanged(s);
    save();
}

void
UserSettings::setDesktopAttentionWindowTitleEnabled(bool s)
{
    if (desktopAttentionWindowTitleEnabled_ == s)
        return;
    desktopAttentionWindowTitleEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::DesktopAttentionWindowTitleEnabled, s);
    emit desktopAttentionWindowTitleEnabledChanged(s);
    save();
}

void
UserSettings::setDesktopAttentionAppBadgeEnabled(bool s)
{
    if (desktopAttentionAppBadgeEnabled_ == s)
        return;
    desktopAttentionAppBadgeEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::DesktopAttentionAppBadgeEnabled, s);
    emit desktopAttentionAppBadgeEnabledChanged(s);
    save();
}
