// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

constexpr std::array<std::pair<UserSettings::RoomSortOrder, const char *>, 4> kRoomSortOrderTokens{{
  {UserSettings::RoomSortOrder::UnreadFirst_Recent, "unread_first_recent"},
  {UserSettings::RoomSortOrder::UnreadFirst_Alpha, "unread_first_alpha"},
  {UserSettings::RoomSortOrder::Recent, "recent"},
  {UserSettings::RoomSortOrder::Alphabetical, "alphabetical"},
}};

constexpr std::array<std::pair<UserSettings::LastMessagePreview, const char *>, 3>
  kLastMessagePreviewTokens{{
    {UserSettings::LastMessagePreview::Always, "always"},
    {UserSettings::LastMessagePreview::OnlyUnencrypted, "only_unencrypted"},
    {UserSettings::LastMessagePreview::Never, "never"},
  }};

QString
toStorageValue(UserSettings::RoomSortOrder value)
{
    return valueToStorageToken(value, kRoomSortOrderTokens, "unread_first_recent");
}

UserSettings::RoomSortOrder
roomSortOrderFromStorage(const QString &value, UserSettings::RoomSortOrder fallback)
{
    return valueFromStorageToken(value, fallback, kRoomSortOrderTokens);
}

QString
toStorageValue(UserSettings::LastMessagePreview value)
{
    return valueToStorageToken(value, kLastMessagePreviewTokens, "always");
}

UserSettings::LastMessagePreview
lastMessagePreviewFromStorage(const QString &value, UserSettings::LastMessagePreview fallback)
{
    return valueFromStorageToken(value, fallback, kLastMessagePreviewTokens);
}

constexpr std::array<std::pair<UserSettings::UnreadDetectionPolicy, const char *>, 2>
  kUnreadDetectionPolicyTokens{{
    {UserSettings::UnreadDetectionPolicy::AnyEvent, "any_event"},
    {UserSettings::UnreadDetectionPolicy::MessagesOnly, "messages_only"},
  }};

QString
toStorageValue(UserSettings::UnreadDetectionPolicy value)
{
    return valueToStorageToken(value, kUnreadDetectionPolicyTokens, "any_event");
}

UserSettings::UnreadDetectionPolicy
unreadDetectionPolicyFromStorage(const QString &value,
                                 UserSettings::UnreadDetectionPolicy fallback)
{
    return valueFromStorageToken(value, fallback, kUnreadDetectionPolicyTokens);
}
