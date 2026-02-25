void
UserSettings::setCallsRelayUseFallbackServer(bool s)
{
    if (callsRelayUseFallbackServer_ == s)
        return;
    callsRelayUseFallbackServer_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsRelayUseFallbackServer, s);
    emit callsRelayUseFallbackServerChanged(s);
    save();
}
void
UserSettings::setCallsLegacyEnabled(bool s)
{
    if (callsLegacyEnabled_ == s)
        return;
    callsLegacyEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsLegacyEnabled, s);
    emit callsLegacyEnabledChanged(s);
    save();
}
void
UserSettings::setEncryptionKeySharingOnlyVerifiedUsers(bool s)
{
    if (encryptionKeySharingOnlyVerifiedUsers_ == s)
        return;
    encryptionKeySharingOnlyVerifiedUsers_ = s;
    (void)coreStore_.set(settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers, s);
    emit encryptionKeySharingOnlyVerifiedUsersChanged(s);
    save();
}
void
UserSettings::setEncryptionKeySharingShareWithTrusted(bool s)
{
    if (encryptionKeySharingShareWithTrusted_ == s)
        return;
    encryptionKeySharingShareWithTrusted_ = s;
    (void)coreStore_.set(settings::core::SettingId::EncryptionKeySharingShareWithTrusted, s);
    emit encryptionKeySharingShareWithTrustedChanged(s);
    save();
}

void
UserSettings::setEncryptionBackupOnlineEnabled(bool useBackup)
{
    if (useBackup == encryptionBackupOnlineEnabled_)
        return;

    encryptionBackupOnlineEnabled_ = useBackup;
    (void)coreStore_.set(settings::core::SettingId::EncryptionBackupOnlineEnabled, useBackup);
    emit encryptionBackupOnlineEnabledChanged(useBackup);
    save();

    if (useBackup)
        olm::download_full_keybackup();
}

void
UserSettings::setEncryptionBackupOnlineEnabledFromConfig(bool useBackup)
{
    if (useBackup == encryptionBackupOnlineEnabled_)
        return;

    encryptionBackupOnlineEnabled_ = useBackup;
    (void)coreStore_.set(settings::core::SettingId::EncryptionBackupOnlineEnabled, useBackup);
    emit encryptionBackupOnlineEnabledChanged(useBackup);
}

void
UserSettings::setCallsAudioRingtone(QString s)
{
    if (callsAudioRingtone_ == s)
        return;
    callsAudioRingtone_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsAudioRingtone, s.toStdString());
    emit callsAudioRingtoneChanged(s);
    save();
}
void
UserSettings::setCallsDevicesMicrophone(QString s)
{
    if (callsDevicesMicrophone_ == s)
        return;
    callsDevicesMicrophone_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsDevicesMicrophone, s.toStdString());
    emit callsDevicesMicrophoneChanged(s);
    save();
}
void
UserSettings::setCallsDevicesCamera(QString s)
{
    if (callsDevicesCamera_ == s)
        return;
    callsDevicesCamera_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsDevicesCamera, s.toStdString());
    emit callsDevicesCameraChanged(s);
    save();
}
void
UserSettings::setCallsDevicesCameraResolution(QString s)
{
    if (callsDevicesCameraResolution_ == s)
        return;
    callsDevicesCameraResolution_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsDevicesCameraResolution, s.toStdString());
    emit callsDevicesCameraResolutionChanged(s);
    save();
}
void
UserSettings::setCallsDevicesCameraFrameRate(QString s)
{
    if (callsDevicesCameraFrameRate_ == s)
        return;
    callsDevicesCameraFrameRate_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsDevicesCameraFrameRate, s.toStdString());
    emit callsDevicesCameraFrameRateChanged(s);
    save();
}
void
UserSettings::setCallsScreenshareFrameRate(int s)
{
    if (callsScreenshareFrameRate_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::CallsScreenshareFrameRate,
                      s,
                      "calls.screenshare.frame_rate"))
        return;
    callsScreenshareFrameRate_ = s;
    emit callsScreenshareFrameRateChanged(s);
    save();
}
void
UserSettings::setCallsScreensharePictureInPicture(bool s)
{
    if (callsScreensharePictureInPicture_ == s)
        return;
    callsScreensharePictureInPicture_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsScreensharePictureInPicture, s);
    emit callsScreensharePictureInPictureChanged(s);
    save();
}
void
UserSettings::setCallsScreenshareIncludeRemoteVideo(bool s)
{
    if (callsScreenshareIncludeRemoteVideo_ == s)
        return;
    callsScreenshareIncludeRemoteVideo_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsScreenshareIncludeRemoteVideo, s);
    emit callsScreenshareIncludeRemoteVideoChanged(s);
    save();
}
void
UserSettings::setCallsScreenshareShowCursor(bool s)
{
    if (callsScreenshareShowCursor_ == s)
        return;
    callsScreenshareShowCursor_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsScreenshareShowCursor, s);
    emit callsScreenshareShowCursorChanged(s);
    save();
}

void
UserSettings::setCurrentTagId(const QString currentTagId)
{
    if (currentTagId == currentTagId_)
        return;
    currentTagId_ = currentTagId;
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
