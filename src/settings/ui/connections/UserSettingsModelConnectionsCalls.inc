    // Calls
    CONNECT_SETTING_ID(CallsLegacyEnabled, callsLegacyEnabledChanged, Value);
    CONNECT_SETTING_ID(CallsElementEnabled, callsElementEnabledChanged, Value);
    CONNECT_SETTING_ID(CallsRelayUseFallbackServer, callsRelayUseFallbackServerChanged, Value);
    CONNECT_SETTING_ID(CallsDevicesMicrophone, callsDevicesMicrophoneChanged, Value, Values);
    CONNECT_SETTING_ID(CallsDevicesCamera, callsDevicesCameraChanged, Value, Values);
    CONNECT_SETTING_ID(CallsDevicesCameraResolution, callsDevicesCameraResolutionChanged, Value, Values);
    CONNECT_SETTING_ID(CallsDevicesCameraFrameRate, callsDevicesCameraFrameRateChanged, Value, Values);
    CONNECT_SETTING_ID(CallsAudioRingtone, callsAudioRingtoneChanged, Values, Value);
    CONNECT_SETTING_ID(CallsScreenshareFrameRate, callsScreenshareFrameRateChanged, Value);
    CONNECT_SETTING_ID(CallsScreensharePictureInPicture, callsScreensharePictureInPictureChanged, Value);
    CONNECT_SETTING_ID(CallsScreenshareIncludeRemoteVideo, callsScreenshareIncludeRemoteVideoChanged, Value);
    CONNECT_SETTING_ID(CallsScreenshareShowCursor, callsScreenshareShowCursorChanged, Value);

    // The "Legacy Calls" subsection rows grey out (Enabled role) when legacy calls
    // are disabled. The per-row Value connections above only refresh each toggle's
    // own value, not the dependent rows' Enabled state, so re-emit Enabled across
    // the whole table when the legacy toggle flips. Only the Calls rows carry an
    // isEnabled predicate, so this is a no-op for every other row.
    connect(settings, &UserSettings::callsLegacyEnabledChanged, this, [this]() {
        const int rows = rowCount();
        if (rows > 0)
            emit dataChanged(index(0), index(rows - 1), {Enabled});
    });
