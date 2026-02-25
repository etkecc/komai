void
UserSettings::setIntegrationsSystemTrayEnabled(bool s)
{
    if (integrationsSystemTrayEnabled_ == s)
        return;
    integrationsSystemTrayEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsSystemTrayEnabled, s);
    emit integrationsSystemTrayEnabledChanged(s);
    save();
}

void
UserSettings::setIntegrationsSystemTrayAutostart(bool s)
{
    if (integrationsSystemTrayAutostart_ == s)
        return;
    integrationsSystemTrayAutostart_ = s;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsSystemTrayAutostart, s);
    emit integrationsSystemTrayAutostartChanged(s);
    save();
}

void
UserSettings::setIntegrationsDbusApiAccess(int access)
{
    if (integrationsDbusApiAccess_ == access)
        return;
    if (!setCoreValue(settings::core::SettingId::IntegrationsDbusApiAccess,
                      access,
                      "integrations.dbus.access"))
        return;
    integrationsDbusApiAccess_ = access;
    emit integrationsDbusApiAccessChanged(access);
    save();
}

void
UserSettings::setIntegrationsBrowserCommand(QString command)
{
    command = command.trimmed();
    if (integrationsBrowserCommand_ == command)
        return;
    integrationsBrowserCommand_ = command;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsBrowserCommand,
                         command.toStdString());
    emit integrationsBrowserCommandChanged(command);
    save();
}
