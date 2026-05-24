// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

constexpr std::array<std::pair<UserSettings::DesktopSystemTrayIconStyle, const char *>, 3>
  kDesktopSystemTrayIconStyleTokens{{
    {UserSettings::DesktopSystemTrayIconStyle::Colorized, "colorized"},
    {UserSettings::DesktopSystemTrayIconStyle::MonochromeLight, "monochrome_light"},
    {UserSettings::DesktopSystemTrayIconStyle::MonochromeDark, "monochrome_dark"},
  }};

QString
toStorageValue(UserSettings::DesktopSystemTrayIconStyle value)
{
    return valueToStorageToken(value, kDesktopSystemTrayIconStyleTokens, "colorized");
}

UserSettings::DesktopSystemTrayIconStyle
desktopSystemTrayIconStyleFromStorage(const QString &value,
                                      UserSettings::DesktopSystemTrayIconStyle fallback)
{
    return valueFromStorageToken(value, fallback, kDesktopSystemTrayIconStyleTokens);
}
