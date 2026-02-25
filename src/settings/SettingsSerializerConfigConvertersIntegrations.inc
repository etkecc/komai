// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

constexpr std::array<std::pair<int, const char *>, 3> kDbusAccessTokens{{
  {IntegrationsDbusAccessNone, "none"},
  {IntegrationsDbusAccessReadOnly, "read_only"},
  {IntegrationsDbusAccessReadWrite, "read_write"},
}};

QString
dbusAccessToStorage(int value)
{
    return valueToStorageToken(value, kDbusAccessTokens, "none");
}

int
dbusAccessFromStorage(const QString &value, int fallback)
{
    return valueFromStorageToken(value, fallback, kDbusAccessTokens);
}
