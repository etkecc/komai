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
