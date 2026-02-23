    // Privacy — WindowFocusBlurEnabled has a side-effect on WindowFocusBlurDelaySeconds's Enabled state
    connect(s.get(), &UserSettings::windowFocusBlurEnabledChanged, this, [this]() {
        if (const int privacyIdx = rowForSettingId(settings::core::SettingId::PrivacyWindowFocusBlurEnabled);
            privacyIdx >= 0) {
            emit dataChanged(index(privacyIdx), index(privacyIdx), {Value});
        }
        if (const int timeoutIdx =
              rowForSettingId(settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds);
            timeoutIdx >= 0) {
            emit dataChanged(index(timeoutIdx), index(timeoutIdx), {Enabled});
        }
    });
    CONNECT_SETTING_ID(PrivacyWindowFocusBlurDelaySeconds, windowFocusBlurDelaySecondsChanged, Value);
    CONNECT_SETTING_ID(PrivacyMaintenanceExpireEvents, expireEventsChanged, Value);
    CONNECT_SETTING_ID(PrivacyMaintenanceUpdateSpaceVias, updateSpaceViasChanged, Value);
