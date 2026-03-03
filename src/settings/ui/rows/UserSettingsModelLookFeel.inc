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
                                    uiFontSizePt,
                                    setUiFontSizePt,
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
      SM::Options, SM::TabLookFeel,
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
    // LookFeelLayoutSection
    { QT_TR_NOOP("Layout"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // MaxContentWidth
    { QT_TR_NOOP("Maximum content width"),
      nullptr,
      SM::IntegerWithDescription, SM::TabLookFeel,
      getCoreIntValue<settings::core::SettingId::UiLayoutContentMaxWidthPx,
                      &UserSettings::uiLayoutContentMaxWidthPx>,
      setSettingValue<&UserSettings::setUiLayoutContentMaxWidthPx, int>,
      0, 20000, 20,
      nullptr,
      nullptr,
      CORE_CONFIG_ID(UiLayoutContentMaxWidthPx, SettingKey::UiLayoutContentMaxWidthPx) },
    // CompactLayout
    { QT_TR_NOOP("Compact mode"),
      QT_TR_NOOP("Affects navigation containers such as header bars and room/sidebar entries, including their icon sizes."),
      SM::ToggleWithDescription, SM::TabLookFeel,
      getCoreBoolValue<settings::core::SettingId::UiLayoutCompactMode,
                       &UserSettings::uiLayoutCompactMode>,
      setSettingValue<&UserSettings::setUiLayoutCompactMode, bool>,
      {}, {}, {},
      nullptr,
      nullptr,
      CORE_CONFIG_ID(UiLayoutCompactMode, SettingKey::UiLayoutCompactMode) },
    // LookFeelAvatarsSection
    { QT_TR_NOOP("Avatars"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CircularAvatarsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING("Use circular avatars",
                        nullptr,
                        SM::TabLookFeel, uiAvatarsCircular, setUiAvatarsCircular, nullptr, UiAvatarsCircular, SettingKey::UiAvatarsCircular),
    // IdenticonFallbackEnabled
    { QT_TR_NOOP("Use identicons"),
      QT_TR_NOOP("Requires the <a href=\"https://jdenticon.com/\">jdenticon</a> plugin; if unavailable in this build, this option is ignored."),
      SM::ToggleWithDescription, SM::TabLookFeel,
      getCoreBoolValue<settings::core::SettingId::UiAvatarsIdenticonFallback,
                       &UserSettings::uiAvatarsIdenticonFallback>,
      setSettingValue<&UserSettings::setUiAvatarsIdenticonFallback, bool>,
      {}, {}, {},
      nullptr,
      identiconSettingEnabled,
      CORE_CONFIG_ID(UiAvatarsIdenticonFallback, SettingKey::UiAvatarsIdenticonFallback) },
    // SidebarsRoomListSection
    { QT_TR_NOOP("Room list"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // RoomListShowLastMessageTime
    SIMPLE_BOOL_CONFIG_ID_SETTING("Show last message timestamp",
                        nullptr,
                        SM::TabSidebars, sidebarsRoomListShowLastMessageTime, setSidebarsRoomListShowLastMessageTime, nullptr, SidebarsRoomListShowLastMessageTime, SettingKey::SidebarsRoomListShowLastMessageTime),
    // ShowLastMessagePreview
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Show last message preview",
      nullptr,
      SM::TabSidebars,
      sidebarsRoomListLastMessagePreview,
      setSidebarsRoomListLastMessagePreview,
      UserSettings::LastMessagePreview,
      sidebarsLastMessagePreviewValues,
      nullptr,
      SidebarsRoomListLastMessagePreview,
      SettingKey::SidebarsRoomListLastMessagePreview),
    // ShowCommunityNotificationCounts
    SIMPLE_BOOL_CONFIG_ID_SETTING("Show notification counts",
                        QT_TR_NOOP("Show total notification counts for communities and tags."),
                        SM::TabSidebars, sidebarsRoomListShowCommunityCounts, setSidebarsRoomListShowCommunityCounts, nullptr, SidebarsRoomListShowCommunityCounts, SettingKey::SidebarsRoomListShowCommunityCounts),
    // RoomListScrollbarsVisible
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show scrollbars",
      nullptr,
      SM::TabSidebars, sidebarsRoomListScrollbarsEnabled, setSidebarsRoomListScrollbarsEnabled, nullptr, SidebarsRoomListScrollbarsEnabled, SettingKey::SidebarsRoomListScrollbarsEnabled),
    // RoomSorting
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Sorting",
      nullptr,
      SM::TabSidebars,
      sidebarsRoomListSort,
      setSidebarsRoomListSort,
      UserSettings::RoomSortOrder,
      sidebarsRoomSortValues,
      nullptr,
      SidebarsRoomListSort,
      SettingKey::SidebarsRoomListSort),
    // LookFeelCommunitiesSidebarSection
    { QT_TR_NOOP("Communities sidebar"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CommunitiesSidebarVisible
    SIMPLE_BOOL_CONFIG_ID_SETTING("Show communities sidebar",
                        nullptr,
                        SM::TabSidebars, sidebarsCommunitiesVisible, setSidebarsCommunitiesVisible, nullptr, SidebarsCommunitiesVisible, SettingKey::SidebarsCommunitiesVisible),
    // IntegrationsSystemTraySection
    { QT_TR_NOOP("System tray"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsTray
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Minimize to tray",
      QT_TR_NOOP("Keep the application running in the background after closing the client window."),
      SM::TabIntegrations, integrationsSystemTrayEnabled, setIntegrationsSystemTrayEnabled, nullptr, IntegrationsSystemTrayEnabled, SettingKey::IntegrationsSystemTrayEnabled),
    // IntegrationsStartInTray
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Start in tray",
      QT_TR_NOOP("Start the application in the background without showing the client window."),
      SM::TabIntegrations, integrationsSystemTrayAutostart, setIntegrationsSystemTrayAutostart, nullptr, IntegrationsSystemTrayAutostart, SettingKey::IntegrationsSystemTrayAutostart),
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
