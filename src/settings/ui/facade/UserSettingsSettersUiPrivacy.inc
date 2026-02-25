void
UserSettings::setPrivacyWindowFocusBlurEnabled(bool s)
{
    if (privacyWindowFocusBlurEnabled_ == s)
        return;
    privacyWindowFocusBlurEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyWindowFocusBlurEnabled, s);
    emit privacyWindowFocusBlurEnabledChanged(s);
    save();
}

void
UserSettings::setPrivacyWindowFocusBlurDelaySeconds(int s)
{
    if (privacyWindowFocusBlurDelaySeconds_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds,
                      s,
                      SettingKey::PrivacyWindowFocusBlurDelaySeconds))
        return;
    privacyWindowFocusBlurDelaySeconds_ = s;
    emit privacyWindowFocusBlurDelaySecondsChanged(s);
    save();
}
