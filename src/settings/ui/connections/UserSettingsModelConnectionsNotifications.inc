    // Notifications
    CONNECT_SETTING_KEY(SettingKey::NotificationsDesktopEnabled, desktopNotificationsChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::NotificationsDesktopAlertOnIncoming, alertOnIncomingMessagesChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::NotificationsDesktopDecryptMessages, decryptNotificationsChanged, Value);
