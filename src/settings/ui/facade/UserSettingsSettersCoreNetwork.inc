void
UserSettings::setNetworkSpacesMaintainJoinMetadata(bool s)
{
    if (networkSpacesMaintainJoinMetadata_ == s)
        return;
    networkSpacesMaintainJoinMetadata_ = s;
    (void)coreStore_.set(settings::core::SettingId::NetworkSpacesMaintainJoinMetadata, s);
    emit networkSpacesMaintainJoinMetadataChanged(s);
    save();
}

void
UserSettings::setNetworkMrsEnabled(bool s)
{
    if (networkMrsEnabled_ == s)
        return;
    networkMrsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::NetworkMrsEnabled, s);
    emit networkMrsEnabledChanged(s);
    save();
}

void
UserSettings::setNetworkMrsServerName(QString serverName)
{
    serverName = serverName.trimmed();
    if (networkMrsServerName_ == serverName)
        return;
    networkMrsServerName_ = serverName;
    (void)coreStore_.set(settings::core::SettingId::NetworkMrsServerName,
                         serverName.toStdString());
    emit networkMrsServerNameChanged(serverName);
    save();
}

void
UserSettings::setNetworkHttp3Enabled(bool s)
{
    if (networkHttp3Enabled_ == s)
        return;
    networkHttp3Enabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::NetworkHttp3Enabled, s);
    emit networkHttp3EnabledChanged(s);
    save();
}
