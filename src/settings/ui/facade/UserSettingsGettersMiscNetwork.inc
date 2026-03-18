bool
UserSettings::networkSpacesMaintainJoinMetadata() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::NetworkSpacesMaintainJoinMetadata);
        value.has_value())
        return *value;
    return networkSpacesMaintainJoinMetadata_;
}

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
UserSettings::networkMrsEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::NetworkMrsEnabled);
        value.has_value())
        return *value;
    return networkMrsEnabled_;
}

QString
UserSettings::networkMrsServerName() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::NetworkMrsServerName);
        value.has_value())
        return QString::fromStdString(*value);
    return networkMrsServerName_;
}

bool
UserSettings::networkHttp3Enabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::NetworkHttp3Enabled);
        value.has_value())
        return *value;
    return networkHttp3Enabled_;
}
