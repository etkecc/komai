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

QString
UserSettings::integrationsTranscriptionProvider() const
{
    if (const auto value = coreStore_.valueAs<std::string>(
          settings::core::SettingId::IntegrationsTranscriptionProvider);
        value.has_value())
        return QString::fromStdString(*value);
    return integrationsTranscriptionProvider_;
}

QString
UserSettings::integrationsTranscriptionApiUrl() const
{
    if (const auto value = coreStore_.valueAs<std::string>(
          settings::core::SettingId::IntegrationsTranscriptionApiUrl);
        value.has_value())
        return QString::fromStdString(*value);
    return integrationsTranscriptionApiUrl_;
}

QString
UserSettings::integrationsTranscriptionModel() const
{
    if (const auto value = coreStore_.valueAs<std::string>(
          settings::core::SettingId::IntegrationsTranscriptionModel);
        value.has_value())
        return QString::fromStdString(*value);
    return integrationsTranscriptionModel_;
}

QString
UserSettings::integrationsTranscriptionLanguage() const
{
    if (const auto value = coreStore_.valueAs<std::string>(
          settings::core::SettingId::IntegrationsTranscriptionLanguage);
        value.has_value())
        return QString::fromStdString(*value);
    return integrationsTranscriptionLanguage_;
}

QString
UserSettings::integrationsTranscriptionPrompt() const
{
    if (const auto value = coreStore_.valueAs<std::string>(
          settings::core::SettingId::IntegrationsTranscriptionPrompt);
        value.has_value())
        return QString::fromStdString(*value);
    return integrationsTranscriptionPrompt_;
}
