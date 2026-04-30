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

QVariantMap
UserSettings::integrationsTranscriptionOverridesForRoom(const QString &roomId) const
{
    QVariantMap result;
    if (roomId.isEmpty())
        return result;
    const auto it = integrationsTranscriptionOverridesByRoom_.find(roomId);
    if (it == integrationsTranscriptionOverridesByRoom_.end())
        return result;
    for (auto fieldIt = it->begin(); fieldIt != it->end(); ++fieldIt)
        result.insert(fieldIt.key(), fieldIt.value());
    return result;
}

bool
UserSettings::hasIntegrationsTranscriptionOverrideForRoom(const QString &roomId,
                                                          const QString &fieldName) const
{
    if (roomId.isEmpty() || fieldName.isEmpty())
        return false;
    const auto it = integrationsTranscriptionOverridesByRoom_.find(roomId);
    if (it == integrationsTranscriptionOverridesByRoom_.end())
        return false;
    return it->contains(fieldName);
}

void
UserSettings::setIntegrationsTranscriptionOverrideForRoom(const QString &roomId,
                                                          const QString &fieldName,
                                                          const QString &value)
{
    if (roomId.isEmpty() || fieldName.isEmpty())
        return;
    auto &perRoom = integrationsTranscriptionOverridesByRoom_[roomId];
    auto existing = perRoom.find(fieldName);
    // For multiline `prompt` we don't trim; everything else is single-line.
    QString stored = (fieldName == QStringLiteral("prompt")) ? value : value.trimmed();
    if (existing != perRoom.end() && *existing == stored)
        return;
    perRoom.insert(fieldName, stored);
    emit integrationsTranscriptionOverridesByRoomChanged();
    save();
}

void
UserSettings::clearIntegrationsTranscriptionOverrideForRoom(const QString &roomId,
                                                            const QString &fieldName)
{
    if (roomId.isEmpty() || fieldName.isEmpty())
        return;
    auto it = integrationsTranscriptionOverridesByRoom_.find(roomId);
    if (it == integrationsTranscriptionOverridesByRoom_.end())
        return;
    if (!it->remove(fieldName))
        return;
    if (it->isEmpty())
        integrationsTranscriptionOverridesByRoom_.erase(it);
    emit integrationsTranscriptionOverridesByRoomChanged();
    save();
}

void
UserSettings::clearIntegrationsTranscriptionOverridesForRoom(const QString &roomId)
{
    if (roomId.isEmpty())
        return;
    if (integrationsTranscriptionOverridesByRoom_.remove(roomId) == 0)
        return;
    emit integrationsTranscriptionOverridesByRoomChanged();
    save();
}

QMap<QString, QMap<QString, QString>>
UserSettings::integrationsTranscriptionOverridesByRoom() const
{
    return integrationsTranscriptionOverridesByRoom_;
}

void
UserSettings::setIntegrationsTranscriptionOverridesByRoom(
  const QMap<QString, QMap<QString, QString>> &byRoom)
{
    if (integrationsTranscriptionOverridesByRoom_ == byRoom)
        return;
    integrationsTranscriptionOverridesByRoom_ = byRoom;
    emit integrationsTranscriptionOverridesByRoomChanged();
}
