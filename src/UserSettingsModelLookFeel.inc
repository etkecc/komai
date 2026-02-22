    // ── Look & Feel Tab ─────────────────────────────────────────────────────

    // LookFeelThemeSection
    { QT_TR_NOOP("THEME"), nullptr, SM::SectionTitle, SM::TabLookFeel,
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
          int idx = v.toInt();
          if (idx >= 0 && idx < slugs.size()) { I->setTheme(slugs.at(idx)); return true; }
          return false;
      },
      {}, {}, {},
      []() -> QVariant {
          auto variant = ThemeRegistry::instance().themeVariant(I->theme());
          if (variant == u"system") return QStringList{};
          return ThemeRegistry::instance().themeNames(variant);
      },
      nullptr },
    // LookFeelFontsSection
    { QT_TR_NOOP("FONTS"), nullptr, SM::SectionTitle, SM::TabLookFeel,
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
          if (v.userType() != QMetaType::Int) return false;
          auto idx = v.toInt();
          I->setFontFamily(idx == 0 ? QString{} : QFontDatabase::families().at(idx - 1));
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          auto fonts = QFontDatabase::families();
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
          return fonts;
      },
      nullptr },
    // FontSize
    { QT_TR_NOOP("Font size"), nullptr, SM::Double, SM::TabLookFeel,
      []() -> QVariant { return I->fontSize(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<double>())) return false;
          I->setFontSize(v.toDouble()); return true;
      },
      8.0, 24.0, 0.5, nullptr, nullptr },
    // EmojiFont
    { QT_TR_NOOP("Emoji font family"), nullptr, SM::Options, SM::TabLookFeel,
      []() -> QVariant {
          if (I->emojiFontFamily().isEmpty()) return 0;
          auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
          fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
          return fonts.indexOf(I->emojiFontFamily());
      },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          auto idx = v.toInt();
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
      nullptr },
#ifndef Q_OS_MACOS
    // ScaleFactor
    { QT_TR_NOOP("Scale factor [restart required]"),
      QT_TR_NOOP("Change the scale factor of the whole user interface. Requires a restart to take effect."),
      SM::Double, SM::TabLookFeel,
      []() -> QVariant { return I->scaleFactor(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert(QMetaType::fromType<double>())) return false;
          I->setScaleFactor(v.toDouble()); return true;
      },
      settings::core::kMinScaleFactor,
      settings::core::kMaxScaleFactor,
      settings::core::kScaleFactorStep,
      nullptr, nullptr },
#endif
    // LookFeelBehaviorSection
    { QT_TR_NOOP("BEHAVIOR"), nullptr, SM::SectionTitle, SM::TabLookFeel,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableUIAnimations
    { QT_TR_NOOP("Enable UI animations"),
      QT_TR_NOOP("Komai uses animations in several places around the interface. Enable or disable them here."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return !I->reducedMotion(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setReducedMotion(!v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // EnableTextSelection
    { QT_TR_NOOP("Enable text selection on timeline"),
      QT_TR_NOOP("Enable text selection in timeline messages. Disable this for a touch-style input "
                 "experience."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return !I->mobileMode(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setMobileMode(!v.toBool());
          return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // EnableSwipeGestures
    { QT_TR_NOOP("Enable swipe gestures"),
      QT_TR_NOOP("Enable swipe gestures like swiping left/right between Rooms and Timeline, or swiping a message to reply."),
      SM::Toggle, SM::TabLookFeel,
      []() -> QVariant { return I->enableSwipeGestures(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setEnableSwipeGestures(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // LookFeelRoomListSection
    { QT_TR_NOOP("ROOM LIST"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CompactRoomList
    { QT_TR_NOOP("Compact mode"),
      QT_TR_NOOP("Use smaller avatars and tighter spacing in the room list and communities sidebar."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->compactRoomList(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setCompactRoomList(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowRoomListTime
    { QT_TR_NOOP("Show last message timestamp"),
      QT_TR_NOOP("Show the timestamp of the last message next to the room name."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->showRoomListTime(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowRoomListTime(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShowLastMessagePreview
    { QT_TR_NOOP("Show last message preview"),
      QT_TR_NOOP("Show a preview of the most recent message in each room."),
      SM::Options, SM::TabSidebars,
      []() -> QVariant { return static_cast<int>(I->showLastMessagePreview()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::LastMessagePreview>().keyCount() <= val) return false;
          I->setShowLastMessagePreview(static_cast<UserSettings::LastMessagePreview>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // ShowCommunityNotificationCounts
    { QT_TR_NOOP("Show notification counts"),
      QT_TR_NOOP("Show total notification counts for communities and tags."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->showCommunityNotificationCounts(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowCommunityNotificationCounts(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseCircularAvatars
    { QT_TR_NOOP("Use circular avatars"),
      QT_TR_NOOP("Change the appearance of user avatars in chats.\nOFF - square, ON - circle."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->useCircularAvatars(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseCircularAvatars(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // UseIdenticon
    { QT_TR_NOOP("Use identicons"),
      QT_TR_NOOP("Display an identicon instead of a letter when no avatar is set."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->useIdenticon(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseIdenticon(v.toBool()); return true;
      },
      {}, {}, {}, nullptr,
      []() -> bool { return JdenticonProvider::isAvailable(); } },
    // ScrollbarsInRoomlist
    { QT_TR_NOOP("Show scrollbars"),
      QT_TR_NOOP("Show scrollbars in the room list and communities sidebar."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->scrollbarsInRoomlist(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setScrollbarsInRoomlist(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // RoomSorting
    { QT_TR_NOOP("Sorting"),
      QT_TR_NOOP("How to order rooms."),
      SM::Options, SM::TabSidebars,
      []() -> QVariant { return static_cast<int>(I->roomSortOrder()); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int) return false;
          I->setRoomSortOrder(static_cast<UserSettings::RoomSortOrder>(v.toInt())); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Unread first, then recent"),
            QCoreApplication::translate("UserSettingsModel", "Unread first, then A-Z"),
            QCoreApplication::translate("UserSettingsModel", "Recent activity"),
            QCoreApplication::translate("UserSettingsModel", "Alphabetical"),
          };
      },
      nullptr },
    // LookFeelCommunitiesSidebarSection
    { QT_TR_NOOP("COMMUNITIES SIDEBAR"), nullptr, SM::SectionTitle, SM::TabSidebars,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ShowCommunitiesSidebar
    { QT_TR_NOOP("Show communities sidebar"),
      QT_TR_NOOP("Show a column containing communities and tags."),
      SM::Toggle, SM::TabSidebars,
      []() -> QVariant { return I->showCommunitiesSidebar(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShowCommunitiesSidebar(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // IntegrationsSystemTraySection
    { QT_TR_NOOP("SYSTEM TRAY"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsTray
    { QT_TR_NOOP("Minimize to tray"),
      QT_TR_NOOP("Keep the application running in the background after closing the client window."),
      SM::Toggle, SM::TabIntegrations,
      []() -> QVariant { return I->tray(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setTray(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // IntegrationsStartInTray
    { QT_TR_NOOP("Start in tray"),
      QT_TR_NOOP("Start the application in the background without showing the client window."),
      SM::Toggle, SM::TabIntegrations,
      []() -> QVariant { return I->startInTray(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setStartInTray(v.toBool()); return true;
      },
      {}, {}, {}, nullptr,
      []() -> bool { return I->tray(); } },
#ifdef NHEKO_DBUS_SYS
    // IntegrationsDbusSection
    { QT_TR_NOOP("D-BUS"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IntegrationsDbusApiAccess
    { QT_TR_NOOP("D-Bus access"),
      QT_TR_NOOP("Choose how much D-Bus access Komai exposes to local callers."),
      SM::Options, SM::TabIntegrations,
      []() -> QVariant { return I->integrationsDbusApiAccess(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Int && !v.canConvert<int>())
              return false;
          auto access = v.toInt();
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
      nullptr },
#endif
    // IntegrationsBrowserSection
    { QT_TR_NOOP("BROWSER"), nullptr, SM::SectionTitle, SM::TabIntegrations,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
