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
