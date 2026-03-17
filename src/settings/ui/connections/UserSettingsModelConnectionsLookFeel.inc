    // Look & Feel
    CONNECT_SETTING_ID(UiThemeSlug, uiThemeSlugChanged, Value, Values, ThemeVariantValue);
    CONNECT_SETTING_ID(UiFontFamily, uiFontFamilyChanged, Value);
    CONNECT_SETTING_ID(UiFontSizePt, uiFontSizePtChanged, Value);
    CONNECT_SETTING_ID(UiFontEmojiFamily, uiFontEmojiFamilyChanged, Value);
    CONNECT_SETTING_ID(UiLayoutContentMaxWidthPx, uiLayoutContentMaxWidthPxChanged, Value);
    CONNECT_SETTING_ID(UiMotionAnimationsEnabled, uiMotionAnimationsEnabledChanged, Value);
    CONNECT_SETTING_ID(UiInputMode, uiInputModeChanged, Value);
    CONNECT_SETTING_ID(UiLayoutCompactMode, uiLayoutCompactModeChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsRoomListShowLastMessageTime, sidebarsRoomListShowLastMessageTimeChanged, Value);
    CONNECT_SETTING_ID(SidebarsRoomListLastMessagePreview, sidebarsRoomListLastMessagePreviewChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsRoomListShowCommunityCounts, sidebarsRoomListShowCommunityCountsChanged, Value);
    CONNECT_SETTING_ID(UiAvatarsCircular, uiAvatarsCircularChanged, Value);
    CONNECT_SETTING_ID(UiAvatarsDefaultAvatarStyle, uiAvatarsDefaultAvatarStyleChanged, Value);
    CONNECT_SETTING_ID(UiScrollbarPolicy, uiScrollbarPolicyChanged, Value);
    CONNECT_SETTING_ID(SidebarsRoomListSort, sidebarsRoomListSortChanged, Value);
    CONNECT_SETTING_ID(SidebarsCommunitiesVisible, sidebarsCommunitiesVisibleChanged, Value);
    // When communities sidebar visibility changes, update Enabled state on filter rows
    // The "All rooms" row is immediately before the first filter row (filterFav - 1).
    if (const int filterFav = rowForSettingId(settings::core::SettingId::SidebarsCommunitiesFilterFavourites),
                 filterLP = rowForSettingId(settings::core::SettingId::SidebarsCommunitiesFilterLowPriority);
        filterFav > 0 && filterLP >= filterFav) {
        const int filterAllRooms = filterFav - 1;
        connect(settings, &UserSettings::sidebarsCommunitiesVisibleChanged, this,
                [this, filterAllRooms, filterLP]() {
                    emit dataChanged(index(filterAllRooms), index(filterLP), {Enabled});
                });
    }
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterFavourites, sidebarsCommunitiesFilterFavouritesChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterPeople, sidebarsCommunitiesFilterPeopleChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterBots, sidebarsCommunitiesFilterBotsChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterGroups, sidebarsCommunitiesFilterGroupsChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterServerNotices, sidebarsCommunitiesFilterServerNoticesChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterLowPriority, sidebarsCommunitiesFilterLowPriorityChanged, Value);
    CONNECT_SETTING_ID(UiInputTouchSwipeGesturesEnabled, uiInputTouchSwipeGesturesEnabledChanged, Value);
