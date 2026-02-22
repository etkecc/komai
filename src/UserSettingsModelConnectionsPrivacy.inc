    // Privacy — PrivacyScreen has a side-effect on PrivacyScreenTimeoutSeconds's Enabled state
    connect(s.get(), &UserSettings::privacyScreenChanged, this, [this]() {
        emit dataChanged(index(PrivacyScreen), index(PrivacyScreen), {Value});
        emit dataChanged(index(PrivacyScreenTimeoutSeconds), index(PrivacyScreenTimeoutSeconds), {Enabled});
    });
    CONNECT_SETTING(PrivacyScreenTimeoutSeconds, privacyScreenTimeoutSecondsChanged, Value);
    CONNECT_SETTING(ExpireEvents, expireEventsChanged, Value);
    CONNECT_SETTING(UpdateSpaceVias, updateSpaceViasChanged, Value);
