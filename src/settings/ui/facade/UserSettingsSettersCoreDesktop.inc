void
UserSettings::setDesktopSystemTrayEnabled(bool s)
{
    if (desktopSystemTrayEnabled_ == s)
        return;
    desktopSystemTrayEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::DesktopSystemTrayEnabled, s);
    emit desktopSystemTrayEnabledChanged(s);
    // Any user-driven toggle of this setting means the user already knows
    // about the tray behavior, so suppress the first-close prompt. Guarded on
    // suppressSettingsSave_ so startup config-load doesn't flip the flag.
    if (!suppressSettingsSave_ && !desktopSystemTrayFirstClosePrompted_) {
        desktopSystemTrayFirstClosePrompted_ = true;
        emit desktopSystemTrayFirstClosePromptedChanged(true);
    }
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
UserSettings::setDesktopSystemTrayIconStyle(DesktopSystemTrayIconStyle s)
{
    if (desktopSystemTrayIconStyle_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::DesktopSystemTrayIconStyle,
                      static_cast<int>(s),
                      SettingKey::DesktopSystemTrayIconStyle))
        return;
    desktopSystemTrayIconStyle_ = s;
    emit desktopSystemTrayIconStyleChanged(s);
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
