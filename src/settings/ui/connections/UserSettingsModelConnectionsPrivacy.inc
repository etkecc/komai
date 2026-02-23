    // Privacy — PrivacyScreen has a side-effect on PrivacyScreenTimeoutSeconds's Enabled state
    connect(s.get(), &UserSettings::privacyScreenChanged, this, [this]() {
        if (const int privacyIdx = rowForSettingId(settings::core::SettingId::PrivacyScreenLockEnabled);
            privacyIdx >= 0) {
            emit dataChanged(index(privacyIdx), index(privacyIdx), {Value});
        }
        if (const int timeoutIdx =
              rowForSettingId(settings::core::SettingId::PrivacyScreenLockTimeoutSeconds);
            timeoutIdx >= 0) {
            emit dataChanged(index(timeoutIdx), index(timeoutIdx), {Enabled});
        }
    });
    CONNECT_SETTING_ID(PrivacyScreenLockTimeoutSeconds, privacyScreenTimeoutSecondsChanged, Value);
    CONNECT_SETTING_ID(PrivacyMaintenanceExpireEvents, expireEventsChanged, Value);
    CONNECT_SETTING_ID(PrivacyMaintenanceUpdateSpaceVias, updateSpaceViasChanged, Value);
