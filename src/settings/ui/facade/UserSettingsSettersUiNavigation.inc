void
UserSettings::setNavigationRoomListSort(RoomSortOrder s)
{
    if (navigationRoomListSort_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NavigationRoomListSort,
                      static_cast<int>(s),
                      "navigation.room_list.sort"))
        return;
    navigationRoomListSort_ = s;
    emit navigationRoomListSortChanged(s);
    save();
}

void
UserSettings::setNavigationRoomListUnreadDetectionPolicy(UnreadDetectionPolicy s)
{
    if (navigationRoomListUnreadDetectionPolicy_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NavigationRoomListUnreadDetectionPolicy,
                      static_cast<int>(s),
                      "navigation.room_list.unread_detection_policy"))
        return;
    navigationRoomListUnreadDetectionPolicy_ = s;
    emit navigationRoomListUnreadDetectionPolicyChanged(s);
    save();
}

void
UserSettings::setNavigationCommunitiesWidthPx(int s)
{
    setSetting(navigationCommunitiesWidthPx_,
               settings::core::definitions::normalizeCommunitiesWidthPx(s),
               &UserSettings::navigationCommunitiesWidthPxChanged);
}

void
UserSettings::setNavigationRoomListWidthPx(int s)
{
    setSetting(navigationRoomListWidthPx_,
               settings::core::definitions::normalizeRoomListWidthPx(s),
               &UserSettings::navigationRoomListWidthPxChanged);
}

void
UserSettings::setNavigationRoomListShowCommunityCounts(bool s)
{
    if (navigationRoomListShowCommunityCounts_ == s)
        return;
    navigationRoomListShowCommunityCounts_ = s;
    (void)coreStore_.set(settings::core::SettingId::NavigationRoomListShowCommunityCounts, s);
    emit navigationRoomListShowCommunityCountsChanged(s);
    save();
}

void
UserSettings::setNavigationRoomListShowLastMessageTime(bool s)
{
    if (navigationRoomListShowLastMessageTime_ == s)
        return;
    navigationRoomListShowLastMessageTime_ = s;
    (void)coreStore_.set(settings::core::SettingId::NavigationRoomListShowLastMessageTime, s);
    emit navigationRoomListShowLastMessageTimeChanged(s);
    save();
}

void
UserSettings::setNavigationRoomListLastMessagePreview(LastMessagePreview s)
{
    if (navigationRoomListLastMessagePreview_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NavigationRoomListLastMessagePreview,
                      static_cast<int>(s),
                      "navigation.room_list.last_message_preview"))
        return;
    navigationRoomListLastMessagePreview_ = s;
    emit navigationRoomListLastMessagePreviewChanged(s);
    save();
}

void
UserSettings::setNavigationTabsShowPinButton(TabPinButtonVisibility s)
{
    if (navigationTabsShowPinButton_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NavigationTabsShowPinButton,
                      static_cast<int>(s),
                      SettingKey::NavigationTabsShowPinButton))
        return;
    navigationTabsShowPinButton_ = s;
    emit navigationTabsShowPinButtonChanged(s);
    save();
}

void
UserSettings::setNavigationTabsPinnedTabLabel(TabLabelDisplay s)
{
    if (navigationTabsPinnedTabLabel_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NavigationTabsPinnedTabLabel,
                      static_cast<int>(s),
                      SettingKey::NavigationTabsPinnedTabLabel))
        return;
    navigationTabsPinnedTabLabel_ = s;
    emit navigationTabsPinnedTabLabelChanged(s);
    save();
}

void
UserSettings::setNavigationTabsTabLabel(TabLabelDisplay s)
{
    if (navigationTabsTabLabel_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NavigationTabsTabLabel,
                      static_cast<int>(s),
                      SettingKey::NavigationTabsTabLabel))
        return;
    navigationTabsTabLabel_ = s;
    emit navigationTabsTabLabelChanged(s);
    save();
}

void
UserSettings::setNavigationTabsPreferredWidthPx(int s)
{
    if (navigationTabsPreferredWidthPx_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NavigationTabsPreferredWidthPx,
                      s,
                      SettingKey::NavigationTabsPreferredWidthPx))
        return;
    navigationTabsPreferredWidthPx_ = s;
    emit navigationTabsPreferredWidthPxChanged(s);
    save();
}

void
UserSettings::setNavigationTabsMinimumWidthPx(int s)
{
    if (navigationTabsMinimumWidthPx_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NavigationTabsMinimumWidthPx,
                      s,
                      SettingKey::NavigationTabsMinimumWidthPx))
        return;
    navigationTabsMinimumWidthPx_ = s;
    emit navigationTabsMinimumWidthPxChanged(s);
    save();
}

void
UserSettings::setNavigationTabsMaxPreRenderedTimelines(int s)
{
    if (navigationTabsMaxPreRenderedTimelines_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NavigationTabsMaxPreRenderedTimelines,
                      s,
                      SettingKey::NavigationTabsMaxPreRenderedTimelines))
        return;
    navigationTabsMaxPreRenderedTimelines_ = s;
    emit navigationTabsMaxPreRenderedTimelinesChanged(s);
    save();
}
