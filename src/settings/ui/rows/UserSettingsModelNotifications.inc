    // ── Notifications Tab ───────────────────────────────────────────────────

    // NotificationsDesktopSection
    { QT_TR_NOOP("Desktop"), nullptr, SM::SectionTitle, SM::TabNotifications,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopNotifications
    SIMPLE_BOOL_CONFIG_SETTING(
      "Desktop notifications",
      QT_TR_NOOP("Notify about received messages when the client is not currently focused."),
      SM::TabNotifications, hasDesktopNotifications, setDesktopNotifications, nullptr, SettingKey::NotificationsDesktopEnabled),
    // AlertOnIncomingMessages
    SIMPLE_BOOL_CONFIG_SETTING(
      "Alert on incoming messages",
      QT_TR_NOOP("Show an alert when a message is received.\nThis usually causes the application icon in the task bar to animate in some fashion."),
      SM::TabNotifications, alertOnIncomingMessages, setAlertOnIncomingMessages, nullptr, SettingKey::NotificationsDesktopAlertOnIncoming),
    // DecryptNotifications
    SIMPLE_BOOL_CONFIG_SETTING("Decrypt notifications",
                        QT_TR_NOOP("Decrypt messages shown in notifications for encrypted chats."),
                        SM::TabNotifications, decryptNotifications, setDecryptNotifications, nullptr, SettingKey::NotificationsDesktopDecryptMessages),
