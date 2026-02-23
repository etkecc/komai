    // ── Notifications Tab ───────────────────────────────────────────────────

    // NotificationsDesktopSection
    { QT_TR_NOOP("Desktop"), nullptr, SM::SectionTitle, SM::TabNotifications,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopNotifications
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Desktop notifications",
      QT_TR_NOOP("Notify about received messages when the client is not currently focused."),
      SM::TabNotifications, desktopNotificationsEnabled, setDesktopNotificationsEnabled, nullptr, NotificationsDesktopEnabled, SettingKey::NotificationsDesktopEnabled),
    // AlertOnIncomingMessages
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Alert on incoming messages",
      QT_TR_NOOP("Show an attention alert when a message is received (for example, taskbar icon activity)."),
      SM::TabNotifications, alertOnIncomingMessages, setAlertOnIncomingMessages, nullptr, NotificationsDesktopAlertOnIncoming, SettingKey::NotificationsDesktopAlertOnIncoming),
    // DecryptNotifications
    SIMPLE_BOOL_CONFIG_ID_SETTING("Decrypt notifications",
                        QT_TR_NOOP("Decrypt messages shown in notifications for encrypted chats."),
                        SM::TabNotifications, decryptNotifications, setDecryptNotifications, nullptr, NotificationsDesktopDecryptMessages, SettingKey::NotificationsDesktopDecryptMessages),
