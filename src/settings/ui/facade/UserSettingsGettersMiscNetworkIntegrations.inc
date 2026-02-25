bool
UserSettings::networkTlsEnableCertificateValidation() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::NetworkTlsEnableCertificateValidation);
        value.has_value())
        return *value;
    return networkTlsEnableCertificateValidation_;
}
bool
UserSettings::timelineMediaOpenImagesExternal() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaOpenImagesExternal);
        value.has_value())
        return *value;
    return timelineMediaOpenImagesExternal_;
}
bool
UserSettings::timelineMediaOpenVideosExternal() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaOpenVideosExternal);
        value.has_value())
        return *value;
    return timelineMediaOpenVideosExternal_;
}
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
bool
UserSettings::privacyMaintenanceUpdateSpaceVias() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias);
        value.has_value())
        return *value;
    return privacyMaintenanceUpdateSpaceVias_;
}
bool
UserSettings::privacyMaintenanceExpireEvents() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyMaintenanceExpireEvents);
        value.has_value())
        return *value;
    return privacyMaintenanceExpireEvents_;
}
// Internal helper: secrets provider is configured via `secrets.provider` in config.yml.
bool
UserSettings::usesFileSecretsProvider() const
{
    return usesFileSecretsProvider_;
}
bool
UserSettings::networkHttp3Enabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::NetworkHttp3Enabled);
        value.has_value())
        return *value;
    return networkHttp3Enabled_;
}
