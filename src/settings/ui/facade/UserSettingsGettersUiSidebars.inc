bool
UserSettings::sidebarsCommunitiesVisible() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::SidebarsCommunitiesVisible);
        value.has_value())
        return *value;
    return sidebarsCommunitiesVisible_;
}

UserSettings::ScrollbarPolicy
UserSettings::uiScrollbarPolicy() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::UiScrollbarPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::ScrollbarPolicy::WhenNeeded) &&
        *value <= static_cast<int>(UserSettings::ScrollbarPolicy::Always))
        return static_cast<UserSettings::ScrollbarPolicy>(*value);
    return uiScrollbarPolicy_;
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
UserSettings::sidebarsCommunitiesFilterFavourites() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::SidebarsCommunitiesFilterFavourites);
        value.has_value())
        return *value;
    return sidebarsCommunitiesFilterFavourites_;
}

bool
UserSettings::sidebarsCommunitiesFilterPeople() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::SidebarsCommunitiesFilterPeople);
        value.has_value())
        return *value;
    return sidebarsCommunitiesFilterPeople_;
}

bool
UserSettings::sidebarsCommunitiesFilterBots() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::SidebarsCommunitiesFilterBots);
        value.has_value())
        return *value;
    return sidebarsCommunitiesFilterBots_;
}

bool
UserSettings::sidebarsCommunitiesFilterGroups() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::SidebarsCommunitiesFilterGroups);
        value.has_value())
        return *value;
    return sidebarsCommunitiesFilterGroups_;
}

bool
UserSettings::sidebarsCommunitiesFilterServerNotices() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::SidebarsCommunitiesFilterServerNotices);
        value.has_value())
        return *value;
    return sidebarsCommunitiesFilterServerNotices_;
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
