// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

constexpr std::array<std::pair<UserSettings::NotificationMessageContentPolicy, const char *>, 3>
  kNotificationMessageContentPolicyTokens{{
    {UserSettings::NotificationMessageContentPolicy::Never, "never"},
    {UserSettings::NotificationMessageContentPolicy::UnencryptedOnly, "unencrypted_only"},
    {UserSettings::NotificationMessageContentPolicy::WheneverAvailable, "whenever_available"},
  }};

QString
toStorageValue(UserSettings::NotificationMessageContentPolicy value)
{
    return valueToStorageToken(
      value, kNotificationMessageContentPolicyTokens, "whenever_available");
}

UserSettings::NotificationMessageContentPolicy
notificationsMessageContentPolicyFromStorage(
  const QString &value,
  UserSettings::NotificationMessageContentPolicy fallback)
{
    return valueFromStorageToken(value, fallback, kNotificationMessageContentPolicyTokens);
}
