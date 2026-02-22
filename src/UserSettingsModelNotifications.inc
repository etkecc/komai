    // ── Notifications Tab ───────────────────────────────────────────────────

    // NotificationsDesktopSection
    { QT_TR_NOOP("DESKTOP"), nullptr, SM::SectionTitle, SM::TabNotifications,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DesktopNotifications
    { QT_TR_NOOP("Desktop notifications"),
      QT_TR_NOOP("Notify about received messages when the client is not currently focused."),
      SM::Toggle, SM::TabNotifications,
    []() -> QVariant { return I->hasDesktopNotifications(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setDesktopNotifications(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // AlertOnIncomingMessages
    { QT_TR_NOOP("Alert on incoming messages"),
      QT_TR_NOOP("Show an alert when a message is received.\nThis usually causes the application icon in the task bar to animate in some fashion."),
      SM::Toggle, SM::TabNotifications,
    []() -> QVariant { return I->alertOnIncomingMessages(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setAlertOnIncomingMessages(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // DecryptNotifications
    { QT_TR_NOOP("Decrypt notifications"),
      QT_TR_NOOP("Decrypt messages shown in notifications for encrypted chats."),
      SM::Toggle, SM::TabNotifications,
    []() -> QVariant { return I->decryptNotifications(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setDecryptNotifications(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
