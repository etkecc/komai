bool
UserSettings::privacyWindowFocusBlurEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyWindowFocusBlurEnabled);
        value.has_value())
        return *value;
    return privacyWindowFocusBlurEnabled_;
}

int
UserSettings::privacyWindowFocusBlurDelaySeconds() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds);
        value.has_value())
        return *value;
    return privacyWindowFocusBlurDelaySeconds_;
}
