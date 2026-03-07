    // ── Notifications Tab ───────────────────────────────────────────────────

    // NotificationsAccountNotificationsSection
    { QT_TR_NOOP("Account notifications"), nullptr, SM::SectionTitle, SM::TabNotifications,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // NotificationsAccountEnabled
    { QT_TR_NOOP("Enable notifications for this account"),
      QT_TR_NOOP("Controls your Matrix account-level notification switch on the homeserver. Turn off to mute notifications across all your clients/devices."),
      SM::Toggle,
      SM::TabNotifications,
      notificationsAccountEnabledValue,
      setNotificationsAccountEnabledValue,
      {},
      {},
      {},
      nullptr,
      notificationsAccountEnabledSettingEnabled,
      CORE_RUNTIME_ID(NotificationsAccountEnabled) },

    // NotificationsSystemNotificationsSection
    { QT_TR_NOOP("System notifications"),
      nullptr,
      SM::SectionTitle,
      SM::TabNotifications,
      nullptr,
      nullptr,
      {},
      {},
      {},
      nullptr,
      nullptr,
      settings::core::SettingId::Unknown,
      nullptr,
      "notifications-system-section" },
    // NotificationsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable system notifications",
      QT_TR_NOOP("Notify about received messages when the client is not currently focused. This only takes effect when account notifications are enabled above."),
      SM::TabNotifications, notificationsEnabled, setNotificationsEnabled, notificationsSystemSettingsEnabled, NotificationsEnabled, SettingKey::NotificationsEnabled),
    // NotificationsMessageContentPolicy
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Message content in notifications",
      QT_TR_NOOP("Choose how much message text to include in notifications."),
      SM::TabNotifications, notificationsMessageContentPolicy, setNotificationsMessageContentPolicy, UserSettings::NotificationMessageContentPolicy,
      notificationsMessageContentPolicyValues,
      notificationsSystemSettingsEnabled,
      NotificationsMessageContentPolicy,
      SettingKey::NotificationsMessageContentPolicy),
    // NotificationsAttentionOnIncoming
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Flash app window/taskbar on incoming messages",
      QT_TR_NOOP("Show an attention alert when a message is received (for example, taskbar icon activity)."),
      SM::TabNotifications, notificationsAttentionOnIncoming, setNotificationsAttentionOnIncoming, notificationsSystemSettingsEnabled, NotificationsAttentionOnIncoming, SettingKey::NotificationsAttentionOnIncoming),
