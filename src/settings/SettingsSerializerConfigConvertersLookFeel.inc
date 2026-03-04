// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
