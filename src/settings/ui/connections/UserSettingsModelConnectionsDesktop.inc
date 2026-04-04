    // Desktop
    CONNECT_SETTING_ID(NotificationsAccountEnabled, notificationsAccountEnabledChanged, Value, Enabled);
    CONNECT_SETTING_ID(NotificationsAccountEnabled, sessionAuthStateChanged, Enabled);
    CONNECT_SETTING_ID(DesktopNotificationsEnabled, sessionAuthStateChanged, Enabled);
    CONNECT_SETTING_ID(DesktopNotificationsEnabled, notificationsAccountEnabledChanged, Enabled);
    CONNECT_SETTING_ID(DesktopNotificationsEnabled, desktopNotificationsEnabledChanged, Value);
    CONNECT_SETTING_ID(DesktopNotificationsAttentionOnIncoming, sessionAuthStateChanged, Enabled);
    CONNECT_SETTING_ID(DesktopNotificationsAttentionOnIncoming,
                       notificationsAccountEnabledChanged,
                       Enabled);
    CONNECT_SETTING_ID(DesktopNotificationsAttentionOnIncoming,
                       desktopNotificationsAttentionOnIncomingChanged,
                       Value);
    CONNECT_SETTING_ID(DesktopNotificationsMessageContentPolicy, sessionAuthStateChanged, Enabled);
    CONNECT_SETTING_ID(DesktopNotificationsMessageContentPolicy,
                       notificationsAccountEnabledChanged,
                       Enabled);
    CONNECT_SETTING_ID(DesktopNotificationsMessageContentPolicy,
                       desktopNotificationsMessageContentPolicyChanged,
                       Value);
    CONNECT_SETTING_ID(DesktopSystemTrayEnabled, desktopSystemTrayEnabledChanged, Value);
    CONNECT_SETTING_ID(DesktopSystemTrayAutostart, desktopSystemTrayAutostartChanged, Value);

    // System tray has a side-effect on Start in tray's Enabled state.
    connect(settings, &UserSettings::desktopSystemTrayEnabledChanged, this, [this]() {
        if (const int trayIdx = rowForSettingId(settings::core::SettingId::DesktopSystemTrayEnabled);
            trayIdx >= 0) {
            emit dataChanged(index(trayIdx), index(trayIdx), {Value});
        }
        if (const int startInTrayIdx =
              rowForSettingId(settings::core::SettingId::DesktopSystemTrayAutostart);
            startInTrayIdx >= 0) {
            emit dataChanged(index(startInTrayIdx), index(startInTrayIdx), {Enabled});
        }
    });

    // Window blur has a side-effect on the delay row's Enabled state.
    connect(settings, &UserSettings::desktopWindowFocusBlurEnabledChanged, this, [this]() {
        if (const int blurIdx =
              rowForSettingId(settings::core::SettingId::DesktopWindowFocusBlurEnabled);
            blurIdx >= 0) {
            emit dataChanged(index(blurIdx), index(blurIdx), {Value});
        }
        if (const int timeoutIdx =
              rowForSettingId(settings::core::SettingId::DesktopWindowFocusBlurDelaySeconds);
            timeoutIdx >= 0) {
            emit dataChanged(index(timeoutIdx), index(timeoutIdx), {Enabled});
        }
    });
    CONNECT_SETTING_ID(DesktopWindowFocusBlurDelaySeconds,
                       desktopWindowFocusBlurDelaySecondsChanged,
                       Value);
