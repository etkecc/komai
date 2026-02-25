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
UserSettings::setRingtone(QString s)
{
    if (ringtone_ == s)
        return;
    ringtone_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsAudioRingtone, s.toStdString());
    emit ringtoneChanged(s);
    save();
}
void
UserSettings::setMicrophone(QString s)
{
    if (microphone_ == s)
        return;
    microphone_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsDevicesMicrophone, s.toStdString());
    emit microphoneChanged(s);
    save();
}
void
UserSettings::setCamera(QString s)
{
    if (camera_ == s)
        return;
    camera_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsDevicesCamera, s.toStdString());
    emit cameraChanged(s);
    save();
}
void
UserSettings::setCameraResolution(QString s)
{
    if (cameraResolution_ == s)
        return;
    cameraResolution_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsDevicesCameraResolution, s.toStdString());
    emit cameraResolutionChanged(s);
    save();
}
void
UserSettings::setCameraFrameRate(QString s)
{
    if (cameraFrameRate_ == s)
        return;
    cameraFrameRate_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsDevicesCameraFrameRate, s.toStdString());
    emit cameraFrameRateChanged(s);
    save();
}
void
UserSettings::setScreenShareFrameRate(int s)
{
    if (screenShareFrameRate_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::CallsScreenshareFrameRate,
                      s,
                      "calls.screenshare.frame_rate"))
        return;
    screenShareFrameRate_ = s;
    emit screenShareFrameRateChanged(s);
    save();
}
void
UserSettings::setScreenSharePiP(bool s)
{
    if (screenSharePiP_ == s)
        return;
    screenSharePiP_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsScreensharePictureInPicture, s);
    emit screenSharePiPChanged(s);
    save();
}
void
UserSettings::setScreenShareRemoteVideo(bool s)
{
    if (screenShareRemoteVideo_ == s)
        return;
    screenShareRemoteVideo_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsScreenshareIncludeRemoteVideo, s);
    emit screenShareRemoteVideoChanged(s);
    save();
}
void
UserSettings::setScreenShareShowCursor(bool s)
{
    if (screenShareShowCursor_ == s)
        return;
    screenShareShowCursor_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsScreenshareShowCursor, s);
    emit screenShareShowCursorChanged(s);
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
UserSettings::setCertificateValidationEnabled(bool enabled)
{
    if (enabled == certificateValidationEnabled_)
        return;
    certificateValidationEnabled_ = enabled;
    (void)coreStore_.set(settings::core::SettingId::NetworkTlsEnableCertificateValidation, enabled);
    http::client()->verify_certificates(enabled);
    emit certificateValidationEnabledChanged(enabled);
    save();
}

void
UserSettings::setIdenticonFallbackEnabled(bool s)
{
    if (identiconFallbackEnabled_ == s)
        return;
    identiconFallbackEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiAvatarsIdenticonFallback, s);
    emit identiconFallbackEnabledChanged(s);
    save();
}
void
UserSettings::setOpenImagesInExternalApp(bool s)
{
    if (openImagesInExternalApp_ == s)
        return;
    openImagesInExternalApp_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaOpenImagesExternal, s);
    emit openImagesInExternalAppChanged(s);
    save();
}
void
UserSettings::setOpenVideosInExternalApp(bool s)
{
    if (openVideosInExternalApp_ == s)
        return;
    openVideosInExternalApp_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaOpenVideosExternal, s);
    emit openVideosInExternalAppChanged(s);
    save();
}
