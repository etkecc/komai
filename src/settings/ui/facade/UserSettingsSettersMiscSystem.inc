void
UserSettings::setCurrentTagId(const QString currentTagId)
{
    if (currentTagId == currentTagId_)
        return;
    currentTagId_ = currentTagId;
    save();
}

void
UserSettings::setCurrentRoomId(const QString currentRoomId)
{
    if (currentRoomId == currentRoomId_)
        return;
    currentRoomId_ = currentRoomId;
    save();
}

void
UserSettings::setNetworkTlsEnableCertificateValidation(bool enabled)
{
    if (enabled == networkTlsEnableCertificateValidation_)
        return;
    networkTlsEnableCertificateValidation_ = enabled;
    (void)coreStore_.set(settings::core::SettingId::NetworkTlsEnableCertificateValidation, enabled);
    http::client()->verify_certificates(enabled);
    emit networkTlsEnableCertificateValidationChanged(enabled);
    save();
}

void
UserSettings::setUiAvatarsIdenticonFallback(bool s)
{
    if (uiAvatarsIdenticonFallback_ == s)
        return;
    uiAvatarsIdenticonFallback_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiAvatarsIdenticonFallback, s);
    emit uiAvatarsIdenticonFallbackChanged(s);
    save();
}
void
UserSettings::setTimelineMediaOpenImagesExternal(bool s)
{
    if (timelineMediaOpenImagesExternal_ == s)
        return;
    timelineMediaOpenImagesExternal_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaOpenImagesExternal, s);
    emit timelineMediaOpenImagesExternalChanged(s);
    save();
}
void
UserSettings::setTimelineMediaOpenVideosExternal(bool s)
{
    if (timelineMediaOpenVideosExternal_ == s)
        return;
    timelineMediaOpenVideosExternal_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaOpenVideosExternal, s);
    emit timelineMediaOpenVideosExternalChanged(s);
    save();
}
