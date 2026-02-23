void
UserSettings::setUseFallbackCallRelayServer(bool s)
{
    setSetting(useFallbackCallRelayServer_, s, &UserSettings::useFallbackCallRelayServerChanged);
}
void
UserSettings::setEnableLegacyCalls(bool s)
{
    setSetting(enableLegacyCalls_, s, &UserSettings::enableLegacyCallsChanged);
}
void
UserSettings::setOnlyShareKeysWithVerifiedUsers(bool s)
{
    setSetting(
      onlyShareKeysWithVerifiedUsers_, s, &UserSettings::onlyShareKeysWithVerifiedUsersChanged);
}
void
UserSettings::setShareKeysWithTrustedUsers(bool s)
{
    setSetting(shareKeysWithTrustedUsers_, s, &UserSettings::shareKeysWithTrustedUsersChanged);
}

void
UserSettings::setUseOnlineKeyBackup(bool useBackup)
{
    if (useBackup == useOnlineKeyBackup_)
        return;

    useOnlineKeyBackup_ = useBackup;
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
    emit useOnlineKeyBackupChanged(useBackup);
}

void
UserSettings::setRingtone(QString s)
{
    setSetting(ringtone_, s, &UserSettings::ringtoneChanged);
}
void
UserSettings::setMicrophone(QString s)
{
    setSetting(microphone_, s, &UserSettings::microphoneChanged);
}
void
UserSettings::setCamera(QString s)
{
    setSetting(camera_, s, &UserSettings::cameraChanged);
}
void
UserSettings::setCameraResolution(QString s)
{
    setSetting(cameraResolution_, s, &UserSettings::cameraResolutionChanged);
}
void
UserSettings::setCameraFrameRate(QString s)
{
    setSetting(cameraFrameRate_, s, &UserSettings::cameraFrameRateChanged);
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
UserSettings::setDisableCertificateValidation(bool disabled)
{
    if (disabled == disableCertificateValidation_)
        return;
    disableCertificateValidation_ = disabled;
    http::client()->verify_certificates(!disabled);
    emit disableCertificateValidationChanged(disabled);
    save();
}

void
UserSettings::setUseIdenticon(bool s)
{
    if (useIdenticon_ == s)
        return;
    useIdenticon_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiAvatarsIdenticonFallback, s);
    emit useIdenticonChanged(s);
    save();
}
void
UserSettings::setOpenImagesInExternalApp(bool s)
{
    setSetting(openImagesInExternalApp_, s, &UserSettings::openImagesInExternalAppChanged);
}
void
UserSettings::setOpenVideosInExternalApp(bool s)
{
    setSetting(openVideosInExternalApp_, s, &UserSettings::openVideosInExternalAppChanged);
}
