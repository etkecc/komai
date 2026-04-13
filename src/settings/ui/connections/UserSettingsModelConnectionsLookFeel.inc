    // Look & Feel
    CONNECT_SETTING_ID(UiThemeSlug, uiThemeSlugChanged, Value, Values, ThemeVariantValue);
    CONNECT_SETTING_ID(UiFontFamily, uiFontFamilyChanged, Value);
    CONNECT_SETTING_ID(UiFontSizePt, uiFontSizePtChanged, Value);
    CONNECT_SETTING_ID(UiFontEmojiFamily, uiFontEmojiFamilyChanged, Value);
CONNECT_SETTING_ID(UiMotionAnimationsEnabled, uiMotionAnimationsEnabledChanged, Value);
    CONNECT_SETTING_ID(UiInputMode, uiInputModeChanged, Value);
    CONNECT_SETTING_ID(UiLayoutCompactMode, uiLayoutCompactModeChanged, Value);
    CONNECT_SETTING_ID(
      NavigationRoomListShowLastMessageTime, navigationRoomListShowLastMessageTimeChanged, Value);
    CONNECT_SETTING_ID(NavigationRoomListLastMessagePreview, navigationRoomListLastMessagePreviewChanged, Value);
    CONNECT_SETTING_ID(
      NavigationRoomListShowCommunityCounts, navigationRoomListShowCommunityCountsChanged, Value);
    CONNECT_SETTING_ID(UiAvatarsCircular, uiAvatarsCircularChanged, Value);
    CONNECT_SETTING_ID(UiAvatarsDefaultAvatarStyle, uiAvatarsDefaultAvatarStyleChanged, Value);
    CONNECT_SETTING_ID(UiScrollbarPolicy, uiScrollbarPolicyChanged, Value);
    CONNECT_SETTING_ID(NavigationRoomListSort, navigationRoomListSortChanged, Value);
    CONNECT_SETTING_ID(NavigationCommunitiesVisible, navigationCommunitiesVisibleChanged, Value);
    // When communities sidebar visibility changes, update Enabled state on filter rows
    // The "All rooms" row is immediately before the first filter row (filterFav - 1).
    if (const int filterFav = rowForSettingId(settings::core::SettingId::NavigationCommunitiesFilterFavourites),
                 filterLP = rowForSettingId(settings::core::SettingId::NavigationCommunitiesFilterLowPriority);
        filterFav > 0 && filterLP >= filterFav) {
        const int filterAllRooms = filterFav - 1;
        connect(settings, &UserSettings::navigationCommunitiesVisibleChanged, this,
                [this, filterAllRooms, filterLP]() {
                    emit dataChanged(index(filterAllRooms), index(filterLP), {Enabled});
                });
    }
    CONNECT_SETTING_ID(
      NavigationCommunitiesFilterFavourites, navigationCommunitiesFilterFavouritesChanged, Value);
    CONNECT_SETTING_ID(
      NavigationCommunitiesFilterPeople, navigationCommunitiesFilterPeopleChanged, Value);
    CONNECT_SETTING_ID(
      NavigationCommunitiesFilterBots, navigationCommunitiesFilterBotsChanged, Value);
    CONNECT_SETTING_ID(
      NavigationCommunitiesFilterGroups, navigationCommunitiesFilterGroupsChanged, Value);
    CONNECT_SETTING_ID(
      NavigationCommunitiesFilterServerNotices, navigationCommunitiesFilterServerNoticesChanged, Value);
    CONNECT_SETTING_ID(
      NavigationCommunitiesFilterLowPriority, navigationCommunitiesFilterLowPriorityChanged, Value);
    CONNECT_SETTING_ID(UiInputTouchSwipeGesturesEnabled, uiInputTouchSwipeGesturesEnabledChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsShowPinButton, navigationTabsShowPinButtonChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsPinnedTabLabel, navigationTabsPinnedTabLabelChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsTabLabel, navigationTabsTabLabelChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsPreferredWidthPx, navigationTabsPreferredWidthPxChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsMinimumWidthPx, navigationTabsMinimumWidthPxChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsMaxPreRenderedTimelines, navigationTabsMaxPreRenderedTimelinesChanged, Value);
