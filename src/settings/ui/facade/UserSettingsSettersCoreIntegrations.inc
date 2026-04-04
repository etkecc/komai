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
