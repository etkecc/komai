    // ── Desktop Tab ─────────────────────────────────────────────────────────

    // DesktopAccountNotificationsSection
    { QT_TR_NOOP("Account notifications"), nullptr, SM::SectionTitle, SM::TabDesktop,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopNotificationsAccountEnabled
    { QT_TR_NOOP("Enable notifications for this account"),
      QT_TR_NOOP("Controls your Matrix account-level notification switch on the homeserver. Turn off to mute notifications across all your clients/devices."),
      SM::Toggle,
      SM::TabDesktop,
      desktopNotificationsAccountEnabledValue,
      setDesktopNotificationsAccountEnabledValue,
      {},
      {},
      {},
      nullptr,
      desktopNotificationsAccountToggleEnabled,
      CORE_RUNTIME_ID(NotificationsAccountEnabled),
      nullptr,  // icon
      nullptr,  // tagId
      true },   // syncedToMatrix

    // DesktopSystemNotificationsSection
    { QT_TR_NOOP("System notifications"),
      nullptr,
      SM::SectionTitle,
      SM::TabDesktop,
      nullptr,
      nullptr,
      {},
      {},
      {},
      nullptr,
      nullptr,
      settings::core::SettingId::Unknown,
      nullptr,
      "desktop-system-notifications-section" },
    // DesktopNotificationsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable system notifications",
      QT_TR_NOOP("Notify about received messages when the client is not currently focused. This only takes effect when account notifications are enabled above."),
      SM::TabDesktop, desktopNotificationsEnabled, setDesktopNotificationsEnabled, desktopNotificationsSettingsEnabled, DesktopNotificationsEnabled, SettingKey::DesktopNotificationsEnabled),
    // DesktopNotificationsMessageContentPolicy
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Message content in notifications",
      QT_TR_NOOP("Choose how much message text to include in notifications."),
      SM::TabDesktop, desktopNotificationsMessageContentPolicy, setDesktopNotificationsMessageContentPolicy, UserSettings::NotificationMessageContentPolicy,
      desktopNotificationsMessageContentPolicyValues,
      desktopNotificationsSettingsEnabled,
      DesktopNotificationsMessageContentPolicy,
      SettingKey::DesktopNotificationsMessageContentPolicy),
    // DesktopNotificationsAttentionOnIncoming
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Flash app window/taskbar on incoming messages",
      QT_TR_NOOP("Show an attention alert when a message is received (for example, taskbar icon activity)."),
      SM::TabDesktop, desktopNotificationsAttentionOnIncoming, setDesktopNotificationsAttentionOnIncoming, desktopNotificationsSettingsEnabled, DesktopNotificationsAttentionOnIncoming, SettingKey::DesktopNotificationsAttentionOnIncoming),

    // DesktopAttentionIndicatorsSection
    { QT_TR_NOOP("Attention indicators"), nullptr, SM::SectionTitle, SM::TabDesktop,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopAttentionWindowTitleEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show attention count in window title",
      QT_TR_NOOP("Append the number of rooms needing attention to the Komai window title."),
      SM::TabDesktop, desktopAttentionWindowTitleEnabled, setDesktopAttentionWindowTitleEnabled, nullptr, DesktopAttentionWindowTitleEnabled, SettingKey::DesktopAttentionWindowTitleEnabled),
    // DesktopAttentionAppBadgeEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show attention count on app icon/taskbar badge",
      QT_TR_NOOP("Show the number of rooms needing attention on the app/taskbar badge."),
      SM::TabDesktop, desktopAttentionAppBadgeEnabled, setDesktopAttentionAppBadgeEnabled, nullptr, DesktopAttentionAppBadgeEnabled, SettingKey::DesktopAttentionAppBadgeEnabled),

    // DesktopSystemTraySection
    { QT_TR_NOOP("System tray"), nullptr, SM::SectionTitle, SM::TabDesktop,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopSystemTrayEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Minimize to tray",
      QT_TR_NOOP("Keep the application running in the background after closing the client window."),
      SM::TabDesktop, desktopSystemTrayEnabled, setDesktopSystemTrayEnabled, nullptr, DesktopSystemTrayEnabled, SettingKey::DesktopSystemTrayEnabled),
    // DesktopSystemTrayAutostart
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Start in tray",
      QT_TR_NOOP("Start the application in the background without showing the client window."),
      SM::TabDesktop, desktopSystemTrayAutostart, setDesktopSystemTrayAutostart, nullptr, DesktopSystemTrayAutostart, SettingKey::DesktopSystemTrayAutostart),

    // DesktopWindowBlurSection
    { QT_TR_NOOP("Window blur"), nullptr, SM::SectionTitle, SM::TabDesktop,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopWindowFocusBlurEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING("Blur on focus loss",
                        nullptr,
                        SM::TabDesktop, desktopWindowFocusBlurEnabled, setDesktopWindowFocusBlurEnabled, nullptr, DesktopWindowFocusBlurEnabled, SettingKey::DesktopWindowFocusBlurEnabled),
    // DesktopWindowFocusBlurDelaySeconds
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(
      "Blur delay (seconds)",
      QT_TR_NOOP(
        "Set how long after focus loss before blurring starts. Set 0 to blur immediately. Maximum is 3600 seconds (1 hour)."),
      SM::TabDesktop, desktopWindowFocusBlurDelaySeconds, setDesktopWindowFocusBlurDelaySeconds, 0, 3600, 10,
      nullptr, desktopWindowFocusBlurDelaySecondsEnabled, DesktopWindowFocusBlurDelaySeconds, SettingKey::DesktopWindowFocusBlurDelaySeconds),
