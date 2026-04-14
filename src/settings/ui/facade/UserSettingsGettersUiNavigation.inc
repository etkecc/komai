bool
UserSettings::navigationCommunitiesVisible() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NavigationCommunitiesVisible);
        value.has_value())
        return *value;
    return navigationCommunitiesVisible_;
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
UserSettings::navigationRoomListShowCommunityCounts() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NavigationRoomListShowCommunityCounts);
        value.has_value())
        return *value;
    return navigationRoomListShowCommunityCounts_;
}

bool
UserSettings::navigationCommunitiesFilterFavourites() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::NavigationCommunitiesFilterFavourites);
        value.has_value())
        return *value;
    return navigationCommunitiesFilterFavourites_;
}

bool
UserSettings::navigationCommunitiesFilterPeople() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::NavigationCommunitiesFilterPeople);
        value.has_value())
        return *value;
    return navigationCommunitiesFilterPeople_;
}

bool
UserSettings::navigationCommunitiesFilterBots() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::NavigationCommunitiesFilterBots);
        value.has_value())
        return *value;
    return navigationCommunitiesFilterBots_;
}

bool
UserSettings::navigationCommunitiesFilterGroups() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::NavigationCommunitiesFilterGroups);
        value.has_value())
        return *value;
    return navigationCommunitiesFilterGroups_;
}

bool
UserSettings::navigationCommunitiesFilterServerNotices() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::NavigationCommunitiesFilterServerNotices);
        value.has_value())
        return *value;
    return navigationCommunitiesFilterServerNotices_;
}

bool
UserSettings::navigationCommunitiesFilterLowPriority() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::NavigationCommunitiesFilterLowPriority);
        value.has_value())
        return *value;
    return navigationCommunitiesFilterLowPriority_;
}

bool
UserSettings::navigationRoomListShowLastMessageTime() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NavigationRoomListShowLastMessageTime);
        value.has_value())
        return *value;
    return navigationRoomListShowLastMessageTime_;
}

UserSettings::LastMessagePreview
UserSettings::navigationRoomListLastMessagePreview() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NavigationRoomListLastMessagePreview);
        value.has_value() && *value >= static_cast<int>(UserSettings::LastMessagePreview::Always) &&
        *value <= static_cast<int>(UserSettings::LastMessagePreview::Never))
        return static_cast<UserSettings::LastMessagePreview>(*value);
    return navigationRoomListLastMessagePreview_;
}

UserSettings::RoomSortOrder
UserSettings::navigationRoomListSort() const
{
    if (const auto value = coreStore_.valueAs<int>(settings::core::SettingId::NavigationRoomListSort);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::RoomSortOrder::UnreadFirst_Recent) &&
        *value <= static_cast<int>(UserSettings::RoomSortOrder::Alphabetical))
        return static_cast<UserSettings::RoomSortOrder>(*value);
    return navigationRoomListSort_;
}

UserSettings::UnreadDetectionPolicy
UserSettings::navigationRoomListUnreadDetectionPolicy() const
{
    if (const auto value = coreStore_.valueAs<int>(
          settings::core::SettingId::NavigationRoomListUnreadDetectionPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::UnreadDetectionPolicy::AnyEvent) &&
        *value <= static_cast<int>(UserSettings::UnreadDetectionPolicy::MessagesOnly))
        return static_cast<UserSettings::UnreadDetectionPolicy>(*value);
    return navigationRoomListUnreadDetectionPolicy_;
}

UserSettings::RoomListOpeningPolicy
UserSettings::navigationRoomListOpeningPolicy() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NavigationRoomListOpeningPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::RoomListOpeningPolicy::ReuseActiveTab) &&
        *value <= static_cast<int>(UserSettings::RoomListOpeningPolicy::OpenNewTab))
        return static_cast<UserSettings::RoomListOpeningPolicy>(*value);
    return navigationRoomListOpeningPolicy_;
}

UserSettings::TabPinButtonVisibility
UserSettings::navigationTabsShowPinButton() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NavigationTabsShowPinButton);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TabPinButtonVisibility::Always) &&
        *value <= static_cast<int>(UserSettings::TabPinButtonVisibility::Never))
        return static_cast<UserSettings::TabPinButtonVisibility>(*value);
    return navigationTabsShowPinButton_;
}

UserSettings::TabLabelDisplay
UserSettings::navigationTabsPinnedTabLabel() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NavigationTabsPinnedTabLabel);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TabLabelDisplay::AvatarAndLabel) &&
        *value <= static_cast<int>(UserSettings::TabLabelDisplay::AvatarOnly))
        return static_cast<UserSettings::TabLabelDisplay>(*value);
    return navigationTabsPinnedTabLabel_;
}

UserSettings::TabLabelDisplay
UserSettings::navigationTabsTabLabel() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NavigationTabsTabLabel);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TabLabelDisplay::AvatarAndLabel) &&
        *value <= static_cast<int>(UserSettings::TabLabelDisplay::AvatarOnly))
        return static_cast<UserSettings::TabLabelDisplay>(*value);
    return navigationTabsTabLabel_;
}

int
UserSettings::navigationTabsPreferredWidthPx() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NavigationTabsPreferredWidthPx);
        value.has_value())
        return *value;
    return navigationTabsPreferredWidthPx_;
}

int
UserSettings::navigationTabsMinimumWidthPx() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NavigationTabsMinimumWidthPx);
        value.has_value())
        return *value;
    return navigationTabsMinimumWidthPx_;
}

int
UserSettings::navigationTabsMaxPreRenderedTimelines() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::NavigationTabsMaxPreRenderedTimelines);
        value.has_value())
        return *value;
    return navigationTabsMaxPreRenderedTimelines_;
}
