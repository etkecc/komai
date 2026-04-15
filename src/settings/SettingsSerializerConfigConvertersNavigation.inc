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

constexpr std::array<std::pair<UserSettings::RoomListOpeningPolicy, const char *>, 2>
  kRoomListOpeningPolicyTokens{{
    {UserSettings::RoomListOpeningPolicy::ReuseActiveTab, "reuse_active_tab"},
    {UserSettings::RoomListOpeningPolicy::OpenNewTab, "open_new_tab"},
  }};

QString
toStorageValue(UserSettings::RoomListOpeningPolicy value)
{
    return valueToStorageToken(value, kRoomListOpeningPolicyTokens, "reuse_active_tab");
}

UserSettings::RoomListOpeningPolicy
roomListOpeningPolicyFromStorage(const QString &value,
                                 UserSettings::RoomListOpeningPolicy fallback)
{
    return valueFromStorageToken(value, fallback, kRoomListOpeningPolicyTokens);
}

constexpr std::array<std::pair<UserSettings::TabPinButtonVisibility, const char *>, 2>
  kTabPinButtonVisibilityTokens{{
    {UserSettings::TabPinButtonVisibility::Always, "always"},
    {UserSettings::TabPinButtonVisibility::Never, "never"},
  }};

QString
toStorageValue(UserSettings::TabPinButtonVisibility value)
{
    return valueToStorageToken(value, kTabPinButtonVisibilityTokens, "never");
}

UserSettings::TabPinButtonVisibility
tabPinButtonVisibilityFromStorage(const QString &value,
                                  UserSettings::TabPinButtonVisibility fallback)
{
    return valueFromStorageToken(value, fallback, kTabPinButtonVisibilityTokens);
}

constexpr std::array<std::pair<UserSettings::TabLabelDisplay, const char *>, 2>
  kTabLabelDisplayTokens{{
    {UserSettings::TabLabelDisplay::AvatarAndLabel, "avatar_and_label"},
    {UserSettings::TabLabelDisplay::AvatarOnly, "avatar_only"},
  }};

QString
toStorageValue(UserSettings::TabLabelDisplay value)
{
    return valueToStorageToken(value, kTabLabelDisplayTokens, "avatar_and_label");
}

UserSettings::TabLabelDisplay
tabLabelDisplayFromStorage(const QString &value, UserSettings::TabLabelDisplay fallback)
{
    return valueFromStorageToken(value, fallback, kTabLabelDisplayTokens);
}
