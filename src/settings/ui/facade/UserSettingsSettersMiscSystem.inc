void
UserSettings::setCurrentFilterId(const QString currentFilterId)
{
    if (currentFilterId == currentFilterId_)
        return;
    currentFilterId_ = currentFilterId;
    scheduleDeferredStateSave();
}

void
UserSettings::setCurrentRoomId(const QString currentRoomId)
{
    if (currentRoomId == currentRoomId_)
        return;
    currentRoomId_ = currentRoomId;
    scheduleDeferredStateSave();
}

void
UserSettings::setNetworkTlsEnableCertificateValidation(bool enabled)
{
    if (enabled == networkTlsEnableCertificateValidation_)
        return;
    networkTlsEnableCertificateValidation_ = enabled;
    (void)coreStore_.set(settings::core::SettingId::NetworkTlsEnableCertificateValidation, enabled);
    emit networkTlsEnableCertificateValidationChanged(enabled);
    save();
}

void
UserSettings::setUiAvatarsDefaultAvatarStyle(DefaultAvatarStyle s)
{
    if (uiAvatarsDefaultAvatarStyle_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::UiAvatarsDefaultAvatarStyle,
                      static_cast<int>(s),
                      "ui.avatars.default_avatar_style"))
        return;
    uiAvatarsDefaultAvatarStyle_ = s;
    emit uiAvatarsDefaultAvatarStyleChanged(s);
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
void
UserSettings::setTimelineMediaAutoplayGifVideos(bool s)
{
    if (timelineMediaAutoplayGifVideos_ == s)
        return;
    timelineMediaAutoplayGifVideos_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaAutoplayGifVideos, s);
    emit timelineMediaAutoplayGifVideosChanged(s);
    save();
}
void
UserSettings::setTimelineMediaDefaultAudioPlaybackSpeed(double speed)
{
    const auto minRate = settings::core::definitions::kMinTimelineMediaAudioPlaybackSpeed;
    const auto maxRate = settings::core::definitions::kMaxTimelineMediaAudioPlaybackSpeed;
    const auto step    = settings::core::definitions::kTimelineMediaAudioPlaybackSpeedStep;
    const auto clamped = std::clamp(speed, minRate, maxRate);
    const auto rounded = std::round(clamped / step) * step;

    if (timelineMediaDefaultAudioPlaybackSpeed_ == rounded)
        return;
    timelineMediaDefaultAudioPlaybackSpeed_ = rounded;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaDefaultAudioPlaybackSpeed,
                         rounded);
    emit timelineMediaDefaultAudioPlaybackSpeedChanged(rounded);
    save();
}
void
UserSettings::setTimelineMediaOpenAudioExternal(bool s)
{
    if (timelineMediaOpenAudioExternal_ == s)
        return;
    timelineMediaOpenAudioExternal_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaOpenAudioExternal, s);
    emit timelineMediaOpenAudioExternalChanged(s);
    save();
}
