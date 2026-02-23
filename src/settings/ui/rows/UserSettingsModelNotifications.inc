    // ── Notifications Tab ───────────────────────────────────────────────────

    // NotificationsDesktopSection
    { nullptr, nullptr, SM::SectionTitle, SM::TabNotifications,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopNotifications
    SIMPLE_BOOL_SETTING(
      "Desktop notifications",
      QT_TR_NOOP("Notify about received messages when the client is not currently focused."),
      SM::TabNotifications, hasDesktopNotifications, setDesktopNotifications, nullptr),
    // AlertOnIncomingMessages
    SIMPLE_BOOL_SETTING(
      "Alert on incoming messages",
      QT_TR_NOOP("Show an alert when a message is received.\nThis usually causes the application icon in the task bar to animate in some fashion."),
      SM::TabNotifications, alertOnIncomingMessages, setAlertOnIncomingMessages, nullptr),
    // DecryptNotifications
    SIMPLE_BOOL_SETTING("Decrypt notifications",
                        QT_TR_NOOP("Decrypt messages shown in notifications for encrypted chats."),
                        SM::TabNotifications, decryptNotifications, setDecryptNotifications, nullptr),
