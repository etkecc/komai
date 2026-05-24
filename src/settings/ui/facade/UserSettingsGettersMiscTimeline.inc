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

bool
UserSettings::timelineMediaAutoplayGifVideos() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaAutoplayGifVideos);
        value.has_value())
        return *value;
    return timelineMediaAutoplayGifVideos_;
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

UserSettings::RoomHeaderButtonLabels
UserSettings::timelineRoomHeaderButtonLabels() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineRoomHeaderButtonLabels);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::RoomHeaderButtonLabels::Adaptive) &&
        *value <= static_cast<int>(UserSettings::RoomHeaderButtonLabels::Never))
        return static_cast<UserSettings::RoomHeaderButtonLabels>(*value);
    return timelineRoomHeaderButtonLabels_;
}
