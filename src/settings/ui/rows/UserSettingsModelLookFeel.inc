    // ── Look & Feel Tab ─────────────────────────────────────────────────────

    // LookFeelThemeSection
    { QT_TR_NOOP("Theme"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Theme
    { QT_TR_NOOP("Theme"),
      QT_TR_NOOP(
        "Need more themes? Import from <a href=\"https://tinted-theming.github.io/tinted-gallery/\">Tinted Gallery</a> or <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/themes.md\">create your own</a> (restart required)."),
      SM::ThemeSelector, SM::TabLookFeel,
      lookFeelThemeValue,
      setLookFeelThemeValue,
      {}, {}, {},
      lookFeelThemeValues,
      nullptr,
      CORE_CONFIG_ID(UiThemeSlug, SettingKey::UiThemeSlug) },
    // LookFeelFontsSection
    { QT_TR_NOOP("Fonts"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Font
    { QT_TR_NOOP("Font family"), nullptr, SM::SearchableOptions, SM::TabLookFeel,
      lookFeelFontFamilyValue,
      setLookFeelFontFamilyValue,
      {}, {}, {},
      lookFeelFontFamilyValues,
      nullptr,
      CORE_CONFIG_ID(UiFontFamily, SettingKey::UiFontFamily) },
    SIMPLE_DOUBLE_CONFIG_ID_SETTING("Font size",
                                    QT_TR_NOOP("Most UI elements resize live. "
                                               "Some labels may only update after restarting the app."),
                                    SM::TabLookFeel,
                                    uiFontSizePt,
                                    setUiFontSizePt,
                                    8.0,
                                    24.0,
                                    0.5,
                                    UiFontSizePt,
                                    SettingKey::UiFontSizePt),
    // EmojiFont
    { QT_TR_NOOP("Emoji font family"), nullptr, SM::SearchableOptions, SM::TabLookFeel,
      lookFeelEmojiFontFamilyValue,
      setLookFeelEmojiFontFamilyValue,
      {}, {}, {},
      lookFeelEmojiFontFamilyValues,
      nullptr,
      CORE_CONFIG_ID(UiFontEmojiFamily, SettingKey::UiFontEmojiFamily) },
#ifndef Q_OS_MACOS
    // ScaleFactor
    SIMPLE_DOUBLE_CONFIG_RESTART_ID_SETTING(
      "Scale factor [restart required]",
      nullptr,
      SM::TabLookFeel, uiScaleFactor, setUiScaleFactor, settings::core::kMinScaleFactor,
      settings::core::kMaxScaleFactor, settings::core::kScaleFactorStep,
      UiScaleFactor, SettingKey::UiScaleFactor),
#endif
    // LookFeelBehaviorSection
    { QT_TR_NOOP("Behavior"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableUIAnimations
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable UI animations",
      nullptr,
      SM::TabLookFeel,
      uiMotionAnimationsEnabled,
      setUiMotionAnimationsEnabled,
      nullptr,
      UiMotionAnimationsEnabled,
      SettingKey::UiMotionAnimationsEnabled),
    // InputMode
    { QT_TR_NOOP("Interaction mode"),
      QT_TR_NOOP("Desktop uses mouse-first behavior. Touch mode favors swipe and touch interactions."),
      SM::SegmentedOptions, SM::TabLookFeel,
      lookFeelInputModeValue,
      setLookFeelInputModeValue,
      {}, {}, {},
      lookFeelInputModeValues,
      nullptr,
      CORE_CONFIG_ID(UiInputMode, SettingKey::UiInputMode) },
    // EnableSwipeGestures
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable swipe gestures",
      QT_TR_NOOP(
        "Enable gestures such as swiping between Room List and Timeline, and swiping a message to reply."),
      SM::TabLookFeel, uiInputTouchSwipeGesturesEnabled, setUiInputTouchSwipeGesturesEnabled, nullptr, UiInputTouchSwipeGesturesEnabled, SettingKey::UiInputTouchSwipeGesturesEnabled),
    // ScrollbarPolicy
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Scrollbar visibility",
      QT_TR_NOOP("Controls when scrollbars are shown across the app."),
      SM::TabLookFeel,
      uiScrollbarPolicy,
      setUiScrollbarPolicy,
      UserSettings::ScrollbarPolicy,
      lookFeelScrollbarPolicyValues,
      nullptr,
      UiScrollbarPolicy,
      SettingKey::UiScrollbarPolicy),
    // LookFeelLayoutSection
    { QT_TR_NOOP("Layout"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Density
    SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(
      "Density",
      QT_TR_NOOP("Affects spacing, icon sizes, and list row heights."),
      SM::TabLookFeel,
      uiLayoutDensity,
      setUiLayoutDensity,
      UserSettings::Density,
      lookFeelDensityValues,
      nullptr,
      UiLayoutDensity,
      SettingKey::UiLayoutDensity),
    // LookFeelAvatarsSection
    { QT_TR_NOOP("Avatars"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AvatarPreview
    { QT_TR_NOOP("Avatar preview"),
      nullptr,
      SM::AvatarPreview, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CircularAvatarsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING("Use circular avatars",
                        nullptr,
                        SM::TabLookFeel, uiAvatarsCircular, setUiAvatarsCircular, nullptr, UiAvatarsCircular, SettingKey::UiAvatarsCircular),
    // DefaultAvatarStyle
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Default avatar style",
      QT_TR_NOOP("Shown for users and rooms that have no custom avatar."),
      SM::TabLookFeel,
      uiAvatarsDefaultAvatarStyle,
      setUiAvatarsDefaultAvatarStyle,
      UserSettings::DefaultAvatarStyle,
      lookFeelDefaultAvatarStyleValues,
      nullptr,
      UiAvatarsDefaultAvatarStyle,
      SettingKey::UiAvatarsDefaultAvatarStyle),
    // LookFeelCommunitiesSidebarSection
    { QT_TR_NOOP("Communities sidebar"), nullptr, SM::SectionTitle, SM::TabNavigation,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr,
      settings::core::SettingId::Unknown, nullptr, "navigation-communities-sidebar-section" },
    // ShowCommunitiesUnreadIndicators
    SIMPLE_BOOL_CONFIG_ID_SETTING("Show unread indicators",
                        QT_TR_NOOP("Show unread emphasis (count badges, row highlight, and left-edge marker) on community and tag filters."),
                        SM::TabNavigation, navigationCommunitiesShowUnreadIndicators, setNavigationCommunitiesShowUnreadIndicators, nullptr, NavigationCommunitiesShowUnreadIndicators, SettingKey::NavigationCommunitiesShowUnreadIndicators),
    // CommunitiesFilterAllRooms (Show/Exclude disabled, Mute enabled)
    {QT_TR_NOOP("All rooms"),
     QT_TR_NOOP("Shows all your rooms in one view. Rooms belonging to filters marked as excluded below will be hidden from this list."),
     SM::CommunityFilterRow,
     SM::TabNavigation,
     nullptr,
     nullptr,
     {},
     {},
     {},
     nullptr,
     nullptr,
     settings::core::SettingId::Unknown,
     ":/icons/icons/ui/world.svg",
     ""},
    // CommunitiesFilterFavourites
    COMMUNITY_FILTER_ROW(
      "Favourites",
      QT_TR_NOOP("Rooms tagged with <a href=\"https://spec.matrix.org/v1.17/client-server-api/#events-14\">m.favourite</a> via the Matrix room tagging API."),
      SM::TabNavigation,
      navigationCommunitiesFilterFavourites,
      setNavigationCommunitiesFilterFavourites,
      nullptr,
      NavigationCommunitiesFilterFavourites,
      SettingKey::NavigationCommunitiesFilterFavourites,
      ":/icons/icons/ui/star.svg",
      "tag:m.favourite"),
    // CommunitiesFilterPeople
    COMMUNITY_FILTER_ROW(
      "People",
      QT_TR_NOOP("Direct chats with real people, excluding users <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/communities-sidebar.md#bot-detection-heuristics\">detected</a> as bots by heuristics."),
      SM::TabNavigation,
      navigationCommunitiesFilterPeople,
      setNavigationCommunitiesFilterPeople,
      nullptr,
      NavigationCommunitiesFilterPeople,
      SettingKey::NavigationCommunitiesFilterPeople,
      ":/icons/icons/ui/person.svg",
      "people"),
    // CommunitiesFilterBots
    COMMUNITY_FILTER_ROW(
      "Bots",
      QT_TR_NOOP("Direct chats where the conversation partner is <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/communities-sidebar.md#bot-detection-heuristics\">detected</a> as a bot or bridge service account."),
      SM::TabNavigation,
      navigationCommunitiesFilterBots,
      setNavigationCommunitiesFilterBots,
      nullptr,
      NavigationCommunitiesFilterBots,
      SettingKey::NavigationCommunitiesFilterBots,
      ":/icons/icons/ui/robot-sparkle.svg",
      "bot"),
    // CommunitiesFilterGroups
    COMMUNITY_FILTER_ROW(
      "Groups",
      QT_TR_NOOP("Multi-participant rooms that are not direct chats (<a href=\"https://spec.matrix.org/v1.17/client-server-api/#direct-messaging\">m.direct</a> and others)."),
      SM::TabNavigation,
      navigationCommunitiesFilterGroups,
      setNavigationCommunitiesFilterGroups,
      nullptr,
      NavigationCommunitiesFilterGroups,
      SettingKey::NavigationCommunitiesFilterGroups,
      ":/icons/icons/ui/people.svg",
      "group"),
    // CommunitiesFilterServerNotices
    COMMUNITY_FILTER_ROW(
      "Server Notices",
      QT_TR_NOOP("Rooms tagged <a href=\"https://spec.matrix.org/v1.17/client-server-api/#events-14\">m.server_notice</a> by the homeserver for administrative messages. See the <a href=\"https://spec.matrix.org/v1.17/client-server-api/#server-notices\">Server Notices</a> module in the Matrix spec."),
      SM::TabNavigation,
      navigationCommunitiesFilterServerNotices,
      setNavigationCommunitiesFilterServerNotices,
      nullptr,
      NavigationCommunitiesFilterServerNotices,
      SettingKey::NavigationCommunitiesFilterServerNotices,
      ":/icons/icons/ui/tag.svg",
      "tag:m.server_notice"),
    // CommunitiesFilterLowPriority
    COMMUNITY_FILTER_ROW(
      "Low Priority",
      QT_TR_NOOP("Rooms tagged with <a href=\"https://spec.matrix.org/v1.17/client-server-api/#events-14\">m.lowpriority</a> via the Matrix room tagging API."),
      SM::TabNavigation,
      navigationCommunitiesFilterLowPriority,
      setNavigationCommunitiesFilterLowPriority,
      nullptr,
      NavigationCommunitiesFilterLowPriority,
      SettingKey::NavigationCommunitiesFilterLowPriority,
      ":/icons/icons/ui/lowprio.svg",
      "tag:m.lowpriority"),
    // SpacesFilterSection (dynamic placeholder for space filter cards)
    {nullptr, nullptr, SM::SpacesFilterSection, SM::TabNavigation,
     nullptr, nullptr, {}, {}, {}, nullptr, nullptr},
    // NavigationRoomListSection
    { QT_TR_NOOP("Room list"), nullptr, SM::SectionTitle, SM::TabNavigation,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr,
      settings::core::SettingId::Unknown, nullptr, "navigation-room-list-section" },
    // ShowRoomListUnreadIndicators
    SIMPLE_BOOL_CONFIG_ID_SETTING("Show unread indicators",
                        QT_TR_NOOP("Show unread emphasis (count badges, bold room name, avatar bounce, row highlight, and left-edge marker) next to rooms."),
                        SM::TabNavigation, navigationRoomListShowUnreadIndicators, setNavigationRoomListShowUnreadIndicators, nullptr, NavigationRoomListShowUnreadIndicators, SettingKey::NavigationRoomListShowUnreadIndicators),
    // RoomListShowLastMessageTime
    SIMPLE_BOOL_CONFIG_ID_SETTING("Show last message timestamp",
                        nullptr,
                        SM::TabNavigation, navigationRoomListShowLastMessageTime, setNavigationRoomListShowLastMessageTime, nullptr, NavigationRoomListShowLastMessageTime, SettingKey::NavigationRoomListShowLastMessageTime),
    // ShowLastMessagePreview
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Show last message preview",
      QT_TR_NOOP("Adds a last-message preview to room list entries. Shown on its own line in Spacious and Compact layouts, and inline next to the room name in Dense."),
      SM::TabNavigation,
      navigationRoomListLastMessagePreview,
      setNavigationRoomListLastMessagePreview,
      UserSettings::LastMessagePreview,
      navigationLastMessagePreviewValues,
      nullptr,
      NavigationRoomListLastMessagePreview,
      SettingKey::NavigationRoomListLastMessagePreview),
    // RoomSorting
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Sorting",
      nullptr,
      SM::TabNavigation,
      navigationRoomListSort,
      setNavigationRoomListSort,
      UserSettings::RoomSortOrder,
      navigationRoomSortValues,
      nullptr,
      NavigationRoomListSort,
      SettingKey::NavigationRoomListSort),
    // RoomListOpeningPolicy
    SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(
      "Room list opening policy",
      QT_TR_NOOP("Whether clicking a room reuses the active tab or opens a new one. Rooms with an existing tab get it activated in all cases."),
      SM::TabNavigation,
      navigationRoomListOpeningPolicy,
      setNavigationRoomListOpeningPolicy,
      UserSettings::RoomListOpeningPolicy,
      navigationRoomListOpeningPolicyValues,
      nullptr,
      NavigationRoomListOpeningPolicy,
      SettingKey::NavigationRoomListOpeningPolicy),
    // NavigationTabBarSection
    { QT_TR_NOOP("Tab bar"), nullptr, SM::SectionTitle, SM::TabNavigation,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr,
      settings::core::SettingId::Unknown, nullptr, "navigation-tab-bar-section" },
    // TabBarShowPinButton
    SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(
      "Show pin button",
      nullptr,
      SM::TabNavigation,
      navigationTabsShowPinButton,
      setNavigationTabsShowPinButton,
      UserSettings::TabPinButtonVisibility,
      navigationTabsShowPinButtonValues,
      nullptr,
      NavigationTabsShowPinButton,
      SettingKey::NavigationTabsShowPinButton),
    // TabBarPinnedTabLabel
    SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(
      "Pinned tab label",
      nullptr,
      SM::TabNavigation,
      navigationTabsPinnedTabLabel,
      setNavigationTabsPinnedTabLabel,
      UserSettings::TabLabelDisplay,
      navigationTabsPinnedTabLabelValues,
      nullptr,
      NavigationTabsPinnedTabLabel,
      SettingKey::NavigationTabsPinnedTabLabel),
    // TabBarTabLabel
    SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(
      "Regular tab label",
      nullptr,
      SM::TabNavigation,
      navigationTabsTabLabel,
      setNavigationTabsTabLabel,
      UserSettings::TabLabelDisplay,
      navigationTabsTabLabelValues,
      nullptr,
      NavigationTabsTabLabel,
      SettingKey::NavigationTabsTabLabel),
    // TabBarPreferredWidthPx
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(
      "Preferred tab width",
      nullptr,
      SM::TabNavigation, navigationTabsPreferredWidthPx, setNavigationTabsPreferredWidthPx, 60, 600, 10,
      nullptr, nullptr, NavigationTabsPreferredWidthPx, SettingKey::NavigationTabsPreferredWidthPx),
    // TabBarMinimumWidthPx
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(
      "Minimum tab width",
      nullptr,
      SM::TabNavigation, navigationTabsMinimumWidthPx, setNavigationTabsMinimumWidthPx, 40, 400, 10,
      nullptr, nullptr, NavigationTabsMinimumWidthPx, SettingKey::NavigationTabsMinimumWidthPx),
    // TabBarMaxRecentlyClosedTimelines
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(
      "Max recently-closed pre-rendered timelines",
      QT_TR_NOOP("Recently-closed pre-rendered timelines enable instant room switching but increase CPU and RAM usage. Open tabs are always pre-rendered. Set to 0 to only keep open tabs."),
      SM::TabNavigation, navigationTabsMaxRecentlyClosedTimelines, setNavigationTabsMaxRecentlyClosedTimelines, 0, 100, 1,
      nullptr, nullptr, NavigationTabsMaxRecentlyClosedTimelines, SettingKey::NavigationTabsMaxRecentlyClosedTimelines),
