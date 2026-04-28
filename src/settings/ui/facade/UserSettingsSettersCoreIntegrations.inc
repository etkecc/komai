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

void
UserSettings::setIntegrationsTranscriptionProvider(QString provider)
{
    provider = provider.trimmed();
    if (integrationsTranscriptionProvider_ == provider)
        return;
    integrationsTranscriptionProvider_ = provider;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsTranscriptionProvider,
                         provider.toStdString());
    emit integrationsTranscriptionProviderChanged(provider);
    save();
}

void
UserSettings::setIntegrationsTranscriptionApiUrl(QString url)
{
    url = url.trimmed();
    if (integrationsTranscriptionApiUrl_ == url)
        return;
    integrationsTranscriptionApiUrl_ = url;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsTranscriptionApiUrl,
                         url.toStdString());
    emit integrationsTranscriptionApiUrlChanged(url);
    save();
}

void
UserSettings::setIntegrationsTranscriptionModel(QString model)
{
    model = model.trimmed();
    if (integrationsTranscriptionModel_ == model)
        return;
    integrationsTranscriptionModel_ = model;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsTranscriptionModel,
                         model.toStdString());
    emit integrationsTranscriptionModelChanged(model);
    save();
}

void
UserSettings::setIntegrationsTranscriptionLanguage(QString language)
{
    language = language.trimmed();
    if (integrationsTranscriptionLanguage_ == language)
        return;
    integrationsTranscriptionLanguage_ = language;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsTranscriptionLanguage,
                         language.toStdString());
    emit integrationsTranscriptionLanguageChanged(language);
    save();
}

void
UserSettings::setIntegrationsTranscriptionPrompt(QString prompt)
{
    // Don't trim — multiline prompts may have intentional leading newlines.
    if (integrationsTranscriptionPrompt_ == prompt)
        return;
    integrationsTranscriptionPrompt_ = prompt;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsTranscriptionPrompt,
                         prompt.toStdString());
    emit integrationsTranscriptionPromptChanged(prompt);
    save();
}
