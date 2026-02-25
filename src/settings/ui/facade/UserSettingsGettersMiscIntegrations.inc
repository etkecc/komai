int
UserSettings::integrationsDbusApiAccess() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::IntegrationsDbusApiAccess);
        value.has_value())
        return *value;
    return integrationsDbusApiAccess_;
}

QString
UserSettings::integrationsBrowserCommand() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::IntegrationsBrowserCommand);
        value.has_value())
        return QString::fromStdString(*value);
    return integrationsBrowserCommand_;
}
