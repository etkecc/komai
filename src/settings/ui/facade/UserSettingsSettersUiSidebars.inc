void
UserSettings::setSidebarsRoomListSort(RoomSortOrder s)
{
    if (sidebarsRoomListSort_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::SidebarsRoomListSort,
                      static_cast<int>(s),
                      "sidebars.room_list.sort"))
        return;
    sidebarsRoomListSort_ = s;
    emit sidebarsRoomListSortChanged(s);
    save();
}

void
UserSettings::setSidebarsCommunitiesWidthPx(int s)
{
    setSetting(sidebarsCommunitiesWidthPx_,
               settings::core::definitions::normalizeCommunitiesWidthPx(s),
               &UserSettings::sidebarsCommunitiesWidthPxChanged);
}

void
UserSettings::setSidebarsRoomListWidthPx(int s)
{
    setSetting(sidebarsRoomListWidthPx_,
               settings::core::definitions::normalizeRoomListWidthPx(s),
               &UserSettings::sidebarsRoomListWidthPxChanged);
}

void
UserSettings::setSidebarsRoomListShowCommunityCounts(bool s)
{
    if (sidebarsRoomListShowCommunityCounts_ == s)
        return;
    sidebarsRoomListShowCommunityCounts_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListShowCommunityCounts, s);
    emit sidebarsRoomListShowCommunityCountsChanged(s);
    save();
}

void
UserSettings::setSidebarsRoomListShowLastMessageTime(bool s)
{
    if (sidebarsRoomListShowLastMessageTime_ == s)
        return;
    sidebarsRoomListShowLastMessageTime_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListShowLastMessageTime, s);
    emit sidebarsRoomListShowLastMessageTimeChanged(s);
    save();
}

void
UserSettings::setSidebarsRoomListLastMessagePreview(LastMessagePreview s)
{
    if (sidebarsRoomListLastMessagePreview_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::SidebarsRoomListLastMessagePreview,
                      static_cast<int>(s),
                      "sidebars.room_list.last_message_preview"))
        return;
    sidebarsRoomListLastMessagePreview_ = s;
    emit sidebarsRoomListLastMessagePreviewChanged(s);
    save();
}
