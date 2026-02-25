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
