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
UserSettings::setNetworkHttp3Enabled(bool s)
{
    if (networkHttp3Enabled_ == s)
        return;
    networkHttp3Enabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::NetworkHttp3Enabled, s);
    emit networkHttp3EnabledChanged(s);
    save();
}
