    // ── Look & Feel Tab ─────────────────────────────────────────────────────

    // LookFeelThemeSection
    { QT_TR_NOOP("Theme"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Theme
    { QT_TR_NOOP("Theme"), nullptr, SM::ThemeSelector, SM::TabLookFeel,
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
    { QT_TR_NOOP("Font family"), nullptr, SM::Options, SM::TabLookFeel,
      lookFeelFontFamilyValue,
      setLookFeelFontFamilyValue,
      {}, {}, {},
      lookFeelFontFamilyValues,
      nullptr,
      CORE_CONFIG_ID(UiFontFamily, SettingKey::UiFontFamily) },
    SIMPLE_DOUBLE_CONFIG_ID_SETTING("Font size",
                                    nullptr,
                                    SM::TabLookFeel,
                                    fontSize,
                                    setFontSize,
                                    8.0,
                                    24.0,
                                    0.5,
                                    UiFontSizePt,
                                    SettingKey::UiFontSizePt),
    // EmojiFont
    { QT_TR_NOOP("Emoji font family"), nullptr, SM::Options, SM::TabLookFeel,
      lookFeelEmojiFontFamilyValue,
      setLookFeelEmojiFontFamilyValue,
      {}, {}, {},
      lookFeelEmojiFontFamilyValues,
      nullptr,
      CORE_CONFIG_ID(UiFontEmojiFamily, SettingKey::UiFontEmojiFamily) },
#ifndef Q_OS_MACOS
    // ScaleFactor
    SIMPLE_DOUBLE_CONFIG_RESTART_SETTING(
      "Scale factor [restart required]",
      QT_TR_NOOP("Change the scale factor of the whole user interface. Requires a restart to take effect."),
      SM::TabLookFeel, scaleFactor, setScaleFactor, settings::core::kMinScaleFactor,
      settings::core::kMaxScaleFactor, settings::core::kScaleFactorStep, SettingKey::UiScaleFactor),
#endif
    // LookFeelBehaviorSection
    { QT_TR_NOOP("Behavior"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableUIAnimations
    SIMPLE_INVERTED_BOOL_CONFIG_ID_SETTING(
      "Enable UI animations",
      QT_TR_NOOP("Komai uses animations in several places around the interface. Enable or disable them here."),
      SM::TabLookFeel,
      reducedMotion,
      setReducedMotion,
      nullptr,
      UiMotionAnimationsEnabled,
      SettingKey::UiMotionAnimationsEnabled),
    // EnableTextSelection
    SIMPLE_INVERTED_BOOL_CONFIG_ID_SETTING(
      "Enable text selection on timeline",
      QT_TR_NOOP("Enable text selection in timeline messages. Disable this for a touch-style input "
                 "experience."),
      SM::TabLookFeel,
      mobileMode,
      setMobileMode,
      nullptr,
      UiInputEnableTextSelection,
      SettingKey::UiInputEnableTextSelection),
    // EnableSwipeGestures
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable swipe gestures",
      QT_TR_NOOP(
        "Enable swipe gestures like swiping left/right between Rooms and Timeline, or swiping a message to reply."),
      SM::TabLookFeel, enableSwipeGestures, setEnableSwipeGestures, nullptr, UiInputSwipeGestures, SettingKey::UiInputSwipeGestures),
    // LookFeelRoomListSection
    { QT_TR_NOOP("Room list"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CompactRoomList
    SIMPLE_BOOL_CONFIG_ID_SETTING("Compact mode",
                        QT_TR_NOOP("Use smaller avatars and tighter spacing in the room list and communities sidebar."),
                        SM::TabSidebars, compactRoomList, setCompactRoomList, nullptr, SidebarsRoomListCompact, SettingKey::SidebarsRoomListCompact),
    // ShowRoomListTime
    SIMPLE_BOOL_CONFIG_ID_SETTING("Show last message timestamp",
                        QT_TR_NOOP("Show the timestamp of the last message next to the room name."),
                        SM::TabSidebars, showRoomListTime, setShowRoomListTime, nullptr, SidebarsRoomListShowLastMessageTime, SettingKey::SidebarsRoomListShowLastMessageTime),
    // ShowLastMessagePreview
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Show last message preview",
      QT_TR_NOOP("Show a preview of the most recent message in each room."),
      SM::TabSidebars,
      showLastMessagePreview,
      setShowLastMessagePreview,
      UserSettings::LastMessagePreview,
      sidebarsLastMessagePreviewValues,
      nullptr,
      SidebarsRoomListLastMessagePreview,
      SettingKey::SidebarsRoomListLastMessagePreview),
    // ShowCommunityNotificationCounts
    SIMPLE_BOOL_CONFIG_ID_SETTING("Show notification counts",
                        QT_TR_NOOP("Show total notification counts for communities and tags."),
                        SM::TabSidebars, showCommunityNotificationCounts, setShowCommunityNotificationCounts, nullptr, SidebarsRoomListShowCommunityCounts, SettingKey::SidebarsRoomListShowCommunityCounts),
    // UseCircularAvatars
    SIMPLE_BOOL_CONFIG_ID_SETTING("Use circular avatars",
                        QT_TR_NOOP("Change the appearance of user avatars in chats.\nOFF - square, ON - circle."),
                        SM::TabSidebars, useCircularAvatars, setUseCircularAvatars, nullptr, UiAvatarsCircular, SettingKey::UiAvatarsCircular),
    // UseIdenticon
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Use identicons",
      QT_TR_NOOP("Display an identicon instead of a letter when no avatar is set."),
      SM::TabSidebars,
      useIdenticon,
      setUseIdenticon,
      identiconSettingEnabled,
      UiAvatarsIdenticonFallback,
      SettingKey::UiAvatarsIdenticonFallback),
    // ScrollbarsInRoomlist
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show scrollbars",
      QT_TR_NOOP("Show scrollbars in the room list and communities sidebar."),
      SM::TabSidebars, scrollbarsInRoomlist, setScrollbarsInRoomlist, nullptr, SidebarsRoomListScrollbarsEnabled, SettingKey::SidebarsRoomListScrollbarsEnabled),
    // RoomSorting
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Sorting",
      QT_TR_NOOP("How to order rooms."),
      SM::TabSidebars,
      roomSortOrder,
      setRoomSortOrder,
      UserSettings::RoomSortOrder,
      sidebarsRoomSortValues,
      nullptr,
      SidebarsRoomListSort,
      SettingKey::SidebarsRoomListSort),
    // LookFeelCommunitiesSidebarSection
    { QT_TR_NOOP("Communities sidebar"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ShowCommunitiesSidebar
    SIMPLE_BOOL_CONFIG_ID_SETTING("Show communities sidebar",
                        QT_TR_NOOP("Show a column containing communities and tags."),
                        SM::TabSidebars, showCommunitiesSidebar, setShowCommunitiesSidebar, nullptr, SidebarsCommunitiesVisible, SettingKey::SidebarsCommunitiesVisible),
    // IntegrationsSystemTraySection
    { QT_TR_NOOP("System tray"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsTray
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Minimize to tray",
      QT_TR_NOOP("Keep the application running in the background after closing the client window."),
      SM::TabIntegrations, tray, setTray, nullptr, IntegrationsSystemTrayEnabled, SettingKey::IntegrationsSystemTrayEnabled),
    // IntegrationsStartInTray
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Start in tray",
      QT_TR_NOOP("Start the application in the background without showing the client window."),
      SM::TabIntegrations, startInTray, setStartInTray, nullptr, IntegrationsSystemTrayAutostart, SettingKey::IntegrationsSystemTrayAutostart),
#ifdef KOMAI_DBUS_SYS
    // IntegrationsDbusSection
    { QT_TR_NOOP("D-Bus"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsDbusApiAccess
    { QT_TR_NOOP("D-Bus access"),
      QT_TR_NOOP("Choose how much D-Bus access Komai exposes to local callers."),
      SM::Options, SM::TabIntegrations,
      integrationsDbusApiAccessValue,
      setIntegrationsDbusApiAccessValue,
      {}, {}, {},
      integrationsDbusApiAccessValues,
      nullptr,
      CORE_CONFIG_ID(IntegrationsDbusApiAccess, SettingKey::IntegrationsDbusApiAccess) },
#endif
    // IntegrationsBrowserSection
    { QT_TR_NOOP("Browser"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
