void
UserSettings::setUseFallbackCallRelayServer(bool s)
{
    if (useFallbackCallRelayServer_ == s)
        return;
    useFallbackCallRelayServer_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsRelayUseFallbackServer, s);
    emit useFallbackCallRelayServerChanged(s);
    save();
}
void
UserSettings::setLegacyCallsEnabled(bool s)
{
    if (legacyCallsEnabled_ == s)
        return;
    legacyCallsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::CallsLegacyEnabled, s);
    emit legacyCallsEnabledChanged(s);
    save();
}
void
UserSettings::setOnlyShareKeysWithVerifiedUsers(bool s)
{
    if (onlyShareKeysWithVerifiedUsers_ == s)
        return;
    onlyShareKeysWithVerifiedUsers_ = s;
    (void)coreStore_.set(settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers, s);
    emit onlyShareKeysWithVerifiedUsersChanged(s);
    save();
}
void
UserSettings::setShareKeysWithTrustedUsers(bool s)
{
    if (shareKeysWithTrustedUsers_ == s)
        return;
    shareKeysWithTrustedUsers_ = s;
    (void)coreStore_.set(settings::core::SettingId::EncryptionKeySharingShareWithTrusted, s);
    emit shareKeysWithTrustedUsersChanged(s);
    save();
}

void
UserSettings::setUseOnlineKeyBackup(bool useBackup)
{
    if (useBackup == useOnlineKeyBackup_)
        return;

    useOnlineKeyBackup_ = useBackup;
    (void)coreStore_.set(settings::core::SettingId::EncryptionBackupOnlineEnabled, useBackup);
    emit useOnlineKeyBackupChanged(useBackup);
    save();

    if (useBackup)
        olm::download_full_keybackup();
}

void
UserSettings::setUseOnlineKeyBackupFromConfig(bool useBackup)
{
    if (useBackup == useOnlineKeyBackup_)
        return;

    useOnlineKeyBackup_ = useBackup;
    (void)coreStore_.set(settings::core::SettingId::EncryptionBackupOnlineEnabled, useBackup);
    emit useOnlineKeyBackupChanged(useBackup);
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
    setSetting(screenShareFrameRate_, s, &UserSettings::screenShareFrameRateChanged);
}
void
UserSettings::setScreenSharePiP(bool s)
{
    setSetting(screenSharePiP_, s, &UserSettings::screenSharePiPChanged);
}
void
UserSettings::setScreenShareRemoteVideo(bool s)
{
    setSetting(screenShareRemoteVideo_, s, &UserSettings::screenShareRemoteVideoChanged);
}
void
UserSettings::setScreenShareHideCursor(bool s)
{
    setSetting(screenShareHideCursor_, s, &UserSettings::screenShareHideCursorChanged);
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
