    // Integrations
    CONNECT_SETTING_ID(IntegrationsSystemTrayEnabled, systemTrayEnabledChanged, Value);
    CONNECT_SETTING_ID(IntegrationsSystemTrayAutostart, systemTrayAutostartChanged, Value);
#ifdef KOMAI_DBUS_SYS
    CONNECT_SETTING_ID(IntegrationsDbusApiAccess, integrationsDbusApiAccessChanged, Value);
#endif

    // Tray has a side-effect on StartInTray's Enabled state
    connect(settings, &UserSettings::systemTrayEnabledChanged, this, [this]() {
        if (const int trayIdx = rowForSettingId(settings::core::SettingId::IntegrationsSystemTrayEnabled);
            trayIdx >= 0) {
            emit dataChanged(index(trayIdx), index(trayIdx), {Value});
        }
        if (const int startInTrayIdx =
              rowForSettingId(settings::core::SettingId::IntegrationsSystemTrayAutostart);
            startInTrayIdx >= 0) {
            emit dataChanged(index(startInTrayIdx), index(startInTrayIdx), {Enabled});
        }
    });
