    // Integrations
    CONNECT_SETTING(IntegrationsTray, trayChanged, Value);
    CONNECT_SETTING(IntegrationsStartInTray, startInTrayChanged, Value);
#ifdef KOMAI_DBUS_SYS
    CONNECT_SETTING(IntegrationsDbusApiAccess, integrationsDbusApiAccessChanged, Value);
#endif

    // Tray has a side-effect on StartInTray's Enabled state
    connect(s.get(), &UserSettings::trayChanged, this, [this]() {
        emit dataChanged(index(IntegrationsTray), index(IntegrationsTray), {Value});
        emit dataChanged(index(IntegrationsStartInTray), index(IntegrationsStartInTray), {Enabled});
    });
