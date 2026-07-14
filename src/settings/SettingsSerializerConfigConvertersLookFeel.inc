// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

constexpr std::array<std::pair<UserSettings::ThemeMode, const char *>, 3>
  kThemeModeTokens{{
    {UserSettings::ThemeMode::Light, "light"},
    {UserSettings::ThemeMode::Dark, "dark"},
    {UserSettings::ThemeMode::Auto, "auto"},
  }};

QString
toStorageValue(UserSettings::ThemeMode value)
{
    return valueToStorageToken(value, kThemeModeTokens, "auto");
}

UserSettings::ThemeMode
themeModeFromStorage(const QString &value, UserSettings::ThemeMode fallback)
{
    return valueFromStorageToken(value, fallback, kThemeModeTokens);
}

constexpr std::array<std::pair<UserSettings::ScrollbarPolicy, const char *>, 3>
  kScrollbarPolicyTokens{{
    {UserSettings::ScrollbarPolicy::WhenNeeded, "when_needed"},
    {UserSettings::ScrollbarPolicy::Never, "never"},
    {UserSettings::ScrollbarPolicy::Always, "always"},
  }};

QString
toStorageValue(UserSettings::ScrollbarPolicy value)
{
    return valueToStorageToken(value, kScrollbarPolicyTokens, "when_needed");
}

UserSettings::ScrollbarPolicy
scrollbarPolicyFromStorage(const QString &value, UserSettings::ScrollbarPolicy fallback)
{
    return valueFromStorageToken(value, fallback, kScrollbarPolicyTokens);
}

constexpr std::array<std::pair<UserSettings::DefaultAvatarStyle, const char *>, 5>
  kDefaultAvatarStyleTokens{{
    {UserSettings::DefaultAvatarStyle::BoringAvatarsBauhaus, "boring_avatars_bauhaus"},
    {UserSettings::DefaultAvatarStyle::BoringAvatarsBeam, "boring_avatars_beam"},
    {UserSettings::DefaultAvatarStyle::BoringAvatarsMarble, "boring_avatars_marble"},
    {UserSettings::DefaultAvatarStyle::LetterInitial, "letter_initial"},
    {UserSettings::DefaultAvatarStyle::UserIcon, "user_icon"},
  }};

QString
toStorageValue(UserSettings::DefaultAvatarStyle value)
{
    return valueToStorageToken(value, kDefaultAvatarStyleTokens, "boring_avatars_bauhaus");
}

UserSettings::DefaultAvatarStyle
defaultAvatarStyleFromStorage(const QString &value, UserSettings::DefaultAvatarStyle fallback)
{
    return valueFromStorageToken(value, fallback, kDefaultAvatarStyleTokens);
}

constexpr std::array<std::pair<UserSettings::Density, const char *>, 3>
  kDensityTokens{{
    {UserSettings::Density::Spacious, "spacious"},
    {UserSettings::Density::Compact, "compact"},
    {UserSettings::Density::Dense, "dense"},
  }};

QString
toStorageValue(UserSettings::Density value)
{
    return valueToStorageToken(value, kDensityTokens, "spacious");
}

UserSettings::Density
densityFromStorage(const QString &value, UserSettings::Density fallback)
{
    return valueFromStorageToken(value, fallback, kDensityTokens);
}
