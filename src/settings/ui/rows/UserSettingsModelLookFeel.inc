    // ── Look & Feel Tab ─────────────────────────────────────────────────────

    // LookFeelThemeSection
    { QT_TR_NOOP("Theme"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Theme
    { QT_TR_NOOP("Theme"), nullptr, SM::ThemeSelector, SM::TabLookFeel,
      []() -> QVariant {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return -1;
          return ThemeRegistry::instance().themeSlugs(variant).indexOf(I->theme());
      },
      [](const QVariant &v) -> bool {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return false;
          auto slugs = ThemeRegistry::instance().themeSlugs(variant);
          int idx = 0;
          if (!readSettingValue(v, idx))
              return false;
          if (idx >= 0 && idx < slugs.size()) { I->setTheme(slugs.at(idx)); return true; }
          return false;
      },
      {}, {}, {},
      []() -> QVariant {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return QStringList{};
          return ThemeRegistry::instance().themeNames(variant);
      },
      nullptr,
      CORE_CONFIG_ID(UiThemeSlug, SettingKey::UiThemeSlug) },
    // LookFeelFontsSection
    { QT_TR_NOOP("Fonts"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Font
    { QT_TR_NOOP("Font family"), nullptr, SM::Options, SM::TabLookFeel,
      []() -> QVariant {
          if (I->font().isEmpty()) return 0;
          auto fonts = QFontDatabase::families();
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
          return fonts.indexOf(I->font());
      },
      [](const QVariant &v) -> bool {
          int idx = 0;
          if (!readSettingValue(v, idx))
              return false;
          I->setFontFamily(idx == 0 ? QString{} : QFontDatabase::families().at(idx - 1));
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          auto fonts = QFontDatabase::families();
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
          return fonts;
      },
      nullptr,
      CORE_CONFIG(SettingKey::UiFontFamily) },
    SIMPLE_DOUBLE_CONFIG_SETTING(
      "Font size", nullptr, SM::TabLookFeel, fontSize, setFontSize, 8.0, 24.0, 0.5, SettingKey::UiFontSizePt),
    // EmojiFont
    { QT_TR_NOOP("Emoji font family"), nullptr, SM::Options, SM::TabLookFeel,
      []() -> QVariant {
          if (I->emojiFontFamily().isEmpty()) return 0;
          auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
          return fonts.indexOf(I->emojiFontFamily());
      },
      [](const QVariant &v) -> bool {
          int idx = 0;
          if (!readSettingValue(v, idx))
              return false;
          I->setEmojiFontFamily(
            idx <= 0 ? QString()
                     : QFontDatabase::families(QFontDatabase::WritingSystem::Symbol).at(idx - 1));
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
          return fonts;
      },
      nullptr,
      CORE_CONFIG(SettingKey::UiFontEmojiFamily) },
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
    SIMPLE_INVERTED_BOOL_CONFIG_SETTING(
      "Enable UI animations",
      QT_TR_NOOP("Komai uses animations in several places around the interface. Enable or disable them here."),
      SM::TabLookFeel,
      reducedMotion,
      setReducedMotion,
      nullptr,
      SettingKey::UiMotionAnimationsEnabled),
    // EnableTextSelection
    SIMPLE_INVERTED_BOOL_CONFIG_SETTING(
      "Enable text selection on timeline",
      QT_TR_NOOP("Enable text selection in timeline messages. Disable this for a touch-style input "
                 "experience."),
      SM::TabLookFeel,
      mobileMode,
      setMobileMode,
      nullptr,
      SettingKey::UiInputEnableTextSelection),
    // EnableSwipeGestures
    SIMPLE_BOOL_CONFIG_SETTING(
      "Enable swipe gestures",
      QT_TR_NOOP(
        "Enable swipe gestures like swiping left/right between Rooms and Timeline, or swiping a message to reply."),
      SM::TabLookFeel, enableSwipeGestures, setEnableSwipeGestures, nullptr, SettingKey::UiInputSwipeGestures),
    // LookFeelRoomListSection
    { QT_TR_NOOP("Room list"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CompactRoomList
    SIMPLE_BOOL_CONFIG_SETTING("Compact mode",
                        QT_TR_NOOP("Use smaller avatars and tighter spacing in the room list and communities sidebar."),
                        SM::TabSidebars, compactRoomList, setCompactRoomList, nullptr, SettingKey::SidebarsRoomListCompact),
    // ShowRoomListTime
    SIMPLE_BOOL_CONFIG_SETTING("Show last message timestamp",
                        QT_TR_NOOP("Show the timestamp of the last message next to the room name."),
                        SM::TabSidebars, showRoomListTime, setShowRoomListTime, nullptr, SettingKey::SidebarsRoomListShowLastMessageTime),
    // ShowLastMessagePreview
    SIMPLE_OPTIONS_ENUM_CONFIG_SETTING(
      "Show last message preview",
      QT_TR_NOOP("Show a preview of the most recent message in each room."),
      SM::TabSidebars,
      showLastMessagePreview,
      setShowLastMessagePreview,
      UserSettings::LastMessagePreview,
      []() -> QVariant {
          QStringList values;
          values << QCoreApplication::translate("UserSettingsModel", "Always")
                 << QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms")
                 << QCoreApplication::translate("UserSettingsModel", "Never");
          return values;
      },
      nullptr,
      SettingKey::SidebarsRoomListLastMessagePreview),
    // ShowCommunityNotificationCounts
    SIMPLE_BOOL_CONFIG_SETTING("Show notification counts",
                        QT_TR_NOOP("Show total notification counts for communities and tags."),
                        SM::TabSidebars, showCommunityNotificationCounts, setShowCommunityNotificationCounts, nullptr, SettingKey::SidebarsRoomListShowCommunityCounts),
    // UseCircularAvatars
    SIMPLE_BOOL_CONFIG_SETTING("Use circular avatars",
                        QT_TR_NOOP("Change the appearance of user avatars in chats.\nOFF - square, ON - circle."),
                        SM::TabSidebars, useCircularAvatars, setUseCircularAvatars, nullptr, SettingKey::UiAvatarsCircular),
    // UseIdenticon
    SIMPLE_BOOL_CONFIG_SETTING(
      "Use identicons",
      QT_TR_NOOP("Display an identicon instead of a letter when no avatar is set."),
      SM::TabSidebars,
      useIdenticon,
      setUseIdenticon,
      []() -> bool { return JdenticonProvider::isAvailable(); },
      SettingKey::UiAvatarsIdenticonFallback),
    // ScrollbarsInRoomlist
    SIMPLE_BOOL_CONFIG_SETTING(
      "Show scrollbars",
      QT_TR_NOOP("Show scrollbars in the room list and communities sidebar."),
      SM::TabSidebars, scrollbarsInRoomlist, setScrollbarsInRoomlist, nullptr, SettingKey::SidebarsRoomListScrollbarsEnabled),
    // RoomSorting
    SIMPLE_OPTIONS_ENUM_CONFIG_SETTING(
      "Sorting",
      QT_TR_NOOP("How to order rooms."),
      SM::TabSidebars,
      roomSortOrder,
      setRoomSortOrder,
      UserSettings::RoomSortOrder,
      []() -> QVariant {
          QStringList values;
          values << QCoreApplication::translate("UserSettingsModel", "Unread first, then recent")
                 << QCoreApplication::translate("UserSettingsModel", "Unread first, then A-Z")
                 << QCoreApplication::translate("UserSettingsModel", "Recent activity")
                 << QCoreApplication::translate("UserSettingsModel", "Alphabetical");
          return values;
      },
      nullptr,
      SettingKey::SidebarsRoomListSort),
    // LookFeelCommunitiesSidebarSection
    { QT_TR_NOOP("Communities sidebar"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ShowCommunitiesSidebar
    SIMPLE_BOOL_CONFIG_SETTING("Show communities sidebar",
                        QT_TR_NOOP("Show a column containing communities and tags."),
                        SM::TabSidebars, showCommunitiesSidebar, setShowCommunitiesSidebar, nullptr, SettingKey::SidebarsCommunitiesVisible),
    // IntegrationsSystemTraySection
    { QT_TR_NOOP("System tray"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsTray
    SIMPLE_BOOL_CONFIG_SETTING(
      "Minimize to tray",
      QT_TR_NOOP("Keep the application running in the background after closing the client window."),
      SM::TabIntegrations, tray, setTray, nullptr, SettingKey::IntegrationsSystemTrayEnabled),
    // IntegrationsStartInTray
    SIMPLE_BOOL_CONFIG_SETTING(
      "Start in tray",
      QT_TR_NOOP("Start the application in the background without showing the client window."),
      SM::TabIntegrations, startInTray, setStartInTray, nullptr, SettingKey::IntegrationsSystemTrayAutostart),
#ifdef KOMAI_DBUS_SYS
    // IntegrationsDbusSection
    { QT_TR_NOOP("D-Bus"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsDbusApiAccess
    { QT_TR_NOOP("D-Bus access"),
      QT_TR_NOOP("Choose how much D-Bus access Komai exposes to local callers."),
      SM::Options, SM::TabIntegrations,
      []() -> QVariant { return I->integrationsDbusApiAccess(); },
      [](const QVariant &v) -> bool {
          int access = 0;
          if (!readSettingValue(v, access))
              return false;
          if (access < IntegrationsDbusAccessNone || access > IntegrationsDbusAccessReadWrite)
              return false;
          I->setIntegrationsDbusApiAccess(access);
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "None"),
            QCoreApplication::translate("UserSettingsModel", "Read-only"),
            QCoreApplication::translate("UserSettingsModel", "Read & write"),
          };
      },
      nullptr,
      CORE_CONFIG(SettingKey::IntegrationsDbusApiAccess) },
#endif
    // IntegrationsBrowserSection
    { QT_TR_NOOP("Browser"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
