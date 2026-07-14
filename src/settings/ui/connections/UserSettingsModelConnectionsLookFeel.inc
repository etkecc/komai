    // Look & Feel
    CONNECT_SETTING_ID(UiThemeSlug, uiThemeSlugChanged, Value, Values, ThemeVariantValue);
    CONNECT_SETTING_ID(UiThemeSlug, uiThemeModeChanged, ThemeVariantValue);
    CONNECT_SETTING_ID(UiFontFamily, uiFontFamilyChanged, Value);
    CONNECT_SETTING_ID(UiFontSizePt, uiFontSizePtChanged, Value);
    CONNECT_SETTING_ID(UiFontEmojiFamily, uiFontEmojiFamilyChanged, Value);
CONNECT_SETTING_ID(UiMotionAnimationsEnabled, uiMotionAnimationsEnabledChanged, Value);
    CONNECT_SETTING_ID(UiLayoutDensity, uiLayoutDensityChanged, Value);
    CONNECT_SETTING_ID(UiLanguage, uiLanguageChanged, Value, Values);
    CONNECT_SETTING_ID(
      NavigationRoomListShowLastMessageTime, navigationRoomListShowLastMessageTimeChanged, Value);
    CONNECT_SETTING_ID(NavigationRoomListLastMessagePreview, navigationRoomListLastMessagePreviewChanged, Value);
    CONNECT_SETTING_ID(
      NavigationRoomListShowUnreadIndicators, navigationRoomListShowUnreadIndicatorsChanged, Value);
    CONNECT_SETTING_ID(NavigationCommunitiesShowUnreadIndicators,
                       navigationCommunitiesShowUnreadIndicatorsChanged,
                       Value);
    CONNECT_SETTING_ID(UiAvatarsCircular, uiAvatarsCircularChanged, Value);
    CONNECT_SETTING_ID(UiAvatarsDefaultAvatarStyle, uiAvatarsDefaultAvatarStyleChanged, Value);
    CONNECT_SETTING_ID(UiScrollbarPolicy, uiScrollbarPolicyChanged, Value);
    CONNECT_SETTING_ID(NavigationRoomListSort, navigationRoomListSortChanged, Value);
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
    CONNECT_SETTING_ID(NavigationRoomListOpeningPolicy, navigationRoomListOpeningPolicyChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsAutoHideSingle, navigationTabsAutoHideSingleChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsShowPinButton, navigationTabsShowPinButtonChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsPinnedTabLabel, navigationTabsPinnedTabLabelChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsTabLabel, navigationTabsTabLabelChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsPreferredWidthPx, navigationTabsPreferredWidthPxChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsMinimumWidthPx, navigationTabsMinimumWidthPxChanged, Value);
    CONNECT_SETTING_ID(NavigationTabsMaxRecentlyClosedTimelines, navigationTabsMaxRecentlyClosedTimelinesChanged, Value);
