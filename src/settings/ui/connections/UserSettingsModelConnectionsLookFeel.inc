    // Look & Feel
    CONNECT_SETTING_KEY(SettingKey::UiThemeSlug, themeChanged, Value, Values, ThemeVariantValue);
    CONNECT_SETTING_KEY(SettingKey::UiFontFamily, fontChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::UiFontSizePt, fontSizeChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::UiFontEmojiFamily, emojiFontChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::UiMotionAnimationsEnabled, reducedMotionChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::SidebarsRoomListCompact, compactRoomListChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::SidebarsRoomListShowLastMessageTime, showRoomListTimeChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::SidebarsRoomListLastMessagePreview, showLastMessagePreviewChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::SidebarsRoomListShowCommunityCounts,
                        showCommunityNotificationCountsChanged,
                        Value);
    CONNECT_SETTING_KEY(SettingKey::UiAvatarsCircular, useCircularAvatarsChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::UiAvatarsIdenticonFallback, useIdenticonChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::SidebarsRoomListScrollbarsEnabled, scrollbarsInRoomlistChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::SidebarsRoomListSort, roomSortOrderChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::SidebarsCommunitiesVisible, showCommunitiesSidebarChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::UiInputEnableTextSelection, mobileModeChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::UiInputSwipeGestures, enableSwipeGesturesChanged, Value);
