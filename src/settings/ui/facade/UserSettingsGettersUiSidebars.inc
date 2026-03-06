bool
UserSettings::sidebarsCommunitiesVisible() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsCommunitiesVisible);
        value.has_value())
        return *value;
    return sidebarsCommunitiesVisible_;
}

bool
UserSettings::sidebarsRoomListScrollbarsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListScrollbarsEnabled);
        value.has_value())
        return *value;
    return sidebarsRoomListScrollbarsEnabled_;
}

bool
UserSettings::sidebarsRoomListShowCommunityCounts() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListShowCommunityCounts);
        value.has_value())
        return *value;
    return sidebarsRoomListShowCommunityCounts_;
}

bool
UserSettings::sidebarsCommunitiesFilterDirectChats() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::SidebarsCommunitiesFilterDirectChats);
        value.has_value())
        return *value;
    return sidebarsCommunitiesFilterDirectChats_;
}

bool
UserSettings::sidebarsCommunitiesFilterFavourites() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::SidebarsCommunitiesFilterFavourites);
        value.has_value())
        return *value;
    return sidebarsCommunitiesFilterFavourites_;
}

bool
UserSettings::sidebarsCommunitiesFilterLowPriority() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::SidebarsCommunitiesFilterLowPriority);
        value.has_value())
        return *value;
    return sidebarsCommunitiesFilterLowPriority_;
}

bool
UserSettings::sidebarsRoomListShowLastMessageTime() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsRoomListShowLastMessageTime);
        value.has_value())
        return *value;
    return sidebarsRoomListShowLastMessageTime_;
}

UserSettings::LastMessagePreview
UserSettings::sidebarsRoomListLastMessagePreview() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::SidebarsRoomListLastMessagePreview);
        value.has_value() && *value >= static_cast<int>(UserSettings::LastMessagePreview::Always) &&
        *value <= static_cast<int>(UserSettings::LastMessagePreview::Never))
        return static_cast<UserSettings::LastMessagePreview>(*value);
    return sidebarsRoomListLastMessagePreview_;
}

UserSettings::RoomSortOrder
UserSettings::sidebarsRoomListSort() const
{
    if (const auto value = coreStore_.valueAs<int>(settings::core::SettingId::SidebarsRoomListSort);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::RoomSortOrder::UnreadFirst_Recent) &&
        *value <= static_cast<int>(UserSettings::RoomSortOrder::Alphabetical))
        return static_cast<UserSettings::RoomSortOrder>(*value);
    return sidebarsRoomListSort_;
}

UserSettings::UnreadDetectionPolicy
UserSettings::sidebarsRoomListUnreadDetectionPolicy() const
{
    if (const auto value = coreStore_.valueAs<int>(
          settings::core::SettingId::SidebarsRoomListUnreadDetectionPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::UnreadDetectionPolicy::AnyEvent) &&
        *value <= static_cast<int>(UserSettings::UnreadDetectionPolicy::MessagesOnly))
        return static_cast<UserSettings::UnreadDetectionPolicy>(*value);
    return sidebarsRoomListUnreadDetectionPolicy_;
}
