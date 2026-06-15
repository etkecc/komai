QString
UserSettings::callsAudioRingtone() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsAudioRingtone);
        value.has_value())
        return QString::fromStdString(*value);
    return callsAudioRingtone_;
}
QString
UserSettings::callsDevicesMicrophone() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesMicrophone);
        value.has_value())
        return QString::fromStdString(*value);
    return callsDevicesMicrophone_;
}
QString
UserSettings::callsDevicesCamera() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesCamera);
        value.has_value())
        return QString::fromStdString(*value);
    return callsDevicesCamera_;
}
QString
UserSettings::callsDevicesCameraResolution() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesCameraResolution);
        value.has_value())
        return QString::fromStdString(*value);
    return callsDevicesCameraResolution_;
}
QString
UserSettings::callsDevicesCameraFrameRate() const
{
    if (const auto value =
          coreStore_.valueAs<std::string>(settings::core::SettingId::CallsDevicesCameraFrameRate);
        value.has_value())
        return QString::fromStdString(*value);
    return callsDevicesCameraFrameRate_;
}
int
UserSettings::callsScreenshareFrameRate() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::CallsScreenshareFrameRate);
        value.has_value())
        return *value;
    return callsScreenshareFrameRate_;
}
bool
UserSettings::callsScreensharePictureInPicture() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::CallsScreensharePictureInPicture);
        value.has_value())
        return *value;
    return callsScreensharePictureInPicture_;
}
bool
UserSettings::callsScreenshareIncludeRemoteVideo() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::CallsScreenshareIncludeRemoteVideo);
        value.has_value())
        return *value;
    return callsScreenshareIncludeRemoteVideo_;
}
bool
UserSettings::callsScreenshareShowCursor() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::CallsScreenshareShowCursor);
        value.has_value())
        return *value;
    return callsScreenshareShowCursor_;
}
bool
UserSettings::callsRelayUseFallbackServer() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::CallsRelayUseFallbackServer);
        value.has_value())
        return *value;
    return callsRelayUseFallbackServer_;
}
bool
UserSettings::callsLegacyEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::CallsLegacyEnabled);
        value.has_value())
        return *value;
    return callsLegacyEnabled_;
}
bool
UserSettings::callsElementEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(settings::core::SettingId::CallsElementEnabled);
        value.has_value())
        return *value;
    return callsElementEnabled_;
}
