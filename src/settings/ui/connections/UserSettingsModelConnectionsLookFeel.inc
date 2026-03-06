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
    CONNECT_SETTING_ID(
      SidebarsRoomListScrollbarsEnabled, sidebarsRoomListScrollbarsEnabledChanged, Value);
    CONNECT_SETTING_ID(SidebarsRoomListSort, sidebarsRoomListSortChanged, Value);
    CONNECT_SETTING_ID(SidebarsCommunitiesVisible, sidebarsCommunitiesVisibleChanged, Value);
    // When communities sidebar visibility changes, update Enabled state on filter rows
    if (const int filterDC = rowForSettingId(settings::core::SettingId::SidebarsCommunitiesFilterDirectChats),
                 filterFav = rowForSettingId(settings::core::SettingId::SidebarsCommunitiesFilterFavourites),
                 filterBots = rowForSettingId(settings::core::SettingId::SidebarsCommunitiesFilterBots),
                 filterSN = rowForSettingId(settings::core::SettingId::SidebarsCommunitiesFilterServerNotices),
                 filterLP = rowForSettingId(settings::core::SettingId::SidebarsCommunitiesFilterLowPriority);
        filterDC >= 0 && filterFav >= 0 && filterBots >= 0 && filterSN >= 0 && filterLP >= 0) {
        connect(settings, &UserSettings::sidebarsCommunitiesVisibleChanged, this,
                [this, filterDC, filterFav, filterBots, filterSN, filterLP]() {
                    emit dataChanged(index(filterDC), index(filterDC), {Enabled});
                    emit dataChanged(index(filterFav), index(filterFav), {Enabled});
                    emit dataChanged(index(filterBots), index(filterBots), {Enabled});
                    emit dataChanged(index(filterSN), index(filterSN), {Enabled});
                    emit dataChanged(index(filterLP), index(filterLP), {Enabled});
                });
    }
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterDirectChats, sidebarsCommunitiesFilterDirectChatsChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterFavourites, sidebarsCommunitiesFilterFavouritesChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterBots, sidebarsCommunitiesFilterBotsChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterServerNotices, sidebarsCommunitiesFilterServerNoticesChanged, Value);
    CONNECT_SETTING_ID(
      SidebarsCommunitiesFilterLowPriority, sidebarsCommunitiesFilterLowPriorityChanged, Value);
    CONNECT_SETTING_ID(UiInputTouchSwipeGesturesEnabled, uiInputTouchSwipeGesturesEnabledChanged, Value);
