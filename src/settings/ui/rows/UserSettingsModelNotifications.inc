    // ── Notifications Tab ───────────────────────────────────────────────────

    // NotificationsSystemNotificationsSection
    { QT_TR_NOOP("System notifications"), nullptr, SM::SectionTitle, SM::TabNotifications,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // NotificationsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable system notifications",
      QT_TR_NOOP("Notify about received messages when the client is not currently focused."),
      SM::TabNotifications, notificationsEnabled, setNotificationsEnabled, nullptr, NotificationsEnabled, SettingKey::NotificationsEnabled),
    // NotificationsMessageContentPolicy
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Message content in notifications",
      QT_TR_NOOP("Choose how much message text to include in notifications."),
      SM::TabNotifications, notificationsMessageContentPolicy, setNotificationsMessageContentPolicy, UserSettings::NotificationMessageContentPolicy,
      notificationsMessageContentPolicyValues,
      nullptr,
      NotificationsMessageContentPolicy,
      SettingKey::NotificationsMessageContentPolicy),
    // NotificationsAttentionOnIncoming
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Flash app window/taskbar on incoming messages",
      QT_TR_NOOP("Show an attention alert when a message is received (for example, taskbar icon activity)."),
      SM::TabNotifications, notificationsAttentionOnIncoming, setNotificationsAttentionOnIncoming, nullptr, NotificationsAttentionOnIncoming, SettingKey::NotificationsAttentionOnIncoming),
