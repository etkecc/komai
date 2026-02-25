// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

constexpr std::array<std::pair<UserSettings::Presence, const char *>, 4> kPresenceTokens{{
  {UserSettings::Presence::AutomaticPresence, "automatic_presence"},
  {UserSettings::Presence::Online, "online"},
  {UserSettings::Presence::Unavailable, "unavailable"},
  {UserSettings::Presence::Offline, "offline"},
}};

QString
toStorageValue(UserSettings::Presence value)
{
    return valueToStorageToken(value, kPresenceTokens, "automatic_presence");
}

UserSettings::Presence
presenceFromStorage(const QString &value, UserSettings::Presence fallback)
{
    return valueFromStorageToken(value, fallback, kPresenceTokens);
}
