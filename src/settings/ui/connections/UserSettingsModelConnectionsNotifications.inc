    // Notifications
    CONNECT_SETTING_ID(NotificationsAccountEnabled, notificationsAccountEnabledChanged, Value, Enabled);
    CONNECT_SETTING_ID(NotificationsAccountEnabled, sessionAuthStateChanged, Enabled);
    CONNECT_SETTING_ID(NotificationsEnabled, sessionAuthStateChanged, Enabled);
    CONNECT_SETTING_ID(NotificationsEnabled, notificationsAccountEnabledChanged, Enabled);
    CONNECT_SETTING_ID(NotificationsEnabled, notificationsEnabledChanged, Value);
    CONNECT_SETTING_ID(NotificationsAttentionOnIncoming, sessionAuthStateChanged, Enabled);
    CONNECT_SETTING_ID(NotificationsAttentionOnIncoming,
                       notificationsAccountEnabledChanged,
                       Enabled);
    CONNECT_SETTING_ID(NotificationsAttentionOnIncoming, notificationsAttentionOnIncomingChanged, Value);
    CONNECT_SETTING_ID(NotificationsMessageContentPolicy, sessionAuthStateChanged, Enabled);
    CONNECT_SETTING_ID(NotificationsMessageContentPolicy,
                       notificationsAccountEnabledChanged,
                       Enabled);
    CONNECT_SETTING_ID(NotificationsMessageContentPolicy, notificationsMessageContentPolicyChanged, Value);
