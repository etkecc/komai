// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

constexpr std::array<std::pair<UserSettings::ShowImage, const char *>, 3> kShowImageTokens{{
  {UserSettings::ShowImage::Always, "always"},
  {UserSettings::ShowImage::OnlyPrivate, "only_private"},
  {UserSettings::ShowImage::Never, "never"},
}};

constexpr std::array<std::pair<UserSettings::ShowSenderUsername, const char *>, 3>
  kShowSenderUsernameTokens{{
    {UserSettings::ShowSenderUsername::Always, "always"},
    {UserSettings::ShowSenderUsername::OnlyInLargeRooms, "only_in_large_rooms"},
    {UserSettings::ShowSenderUsername::Never, "never"},
  }};

QString
toStorageValue(UserSettings::ShowImage value)
{
    return valueToStorageToken(value, kShowImageTokens, "always");
}

UserSettings::ShowImage
showImageFromStorage(const QString &value, UserSettings::ShowImage fallback)
{
    return valueFromStorageToken(value, fallback, kShowImageTokens);
}

QString
toStorageValue(UserSettings::ShowSenderUsername value)
{
    return valueToStorageToken(value, kShowSenderUsernameTokens, "only_in_large_rooms");
}

UserSettings::ShowSenderUsername
showSenderUsernameFromStorage(const QString &value, UserSettings::ShowSenderUsername fallback)
{
    return valueFromStorageToken(value, fallback, kShowSenderUsernameTokens);
}
