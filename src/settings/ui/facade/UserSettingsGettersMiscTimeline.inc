bool
UserSettings::timelineMediaOpenImagesExternal() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaOpenImagesExternal);
        value.has_value())
        return *value;
    return timelineMediaOpenImagesExternal_;
}

bool
UserSettings::timelineMediaOpenVideosExternal() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaOpenVideosExternal);
        value.has_value())
        return *value;
    return timelineMediaOpenVideosExternal_;
}

double
UserSettings::timelineMediaDefaultAudioPlaybackSpeed() const
{
    if (const auto value = coreStore_.valueAs<double>(
          settings::core::SettingId::TimelineMediaDefaultAudioPlaybackSpeed);
        value.has_value())
        return *value;
    return timelineMediaDefaultAudioPlaybackSpeed_;
}

bool
UserSettings::timelineMediaOpenAudioExternal() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaOpenAudioExternal);
        value.has_value())
        return *value;
    return timelineMediaOpenAudioExternal_;
}
