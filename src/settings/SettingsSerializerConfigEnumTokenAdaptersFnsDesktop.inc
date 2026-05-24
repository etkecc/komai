QString
desktopSystemTrayIconStyleToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.desktopSystemTrayIconStyle());
}

void
applyDesktopSystemTrayIconStyleFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setDesktopSystemTrayIconStyle(desktopSystemTrayIconStyleFromStorage(
      rawToken, UserSettings::DesktopSystemTrayIconStyle::Colorized));
}
