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
UserSettings::networkHttp3Enabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::NetworkHttp3Enabled);
        value.has_value())
        return *value;
    return networkHttp3Enabled_;
}
