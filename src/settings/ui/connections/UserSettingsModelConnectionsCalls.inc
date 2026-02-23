    // Calls
    CONNECT_SETTING_ID(CallsLegacyEnabled, legacyCallsEnabledChanged, Value);
    CONNECT_SETTING_ID(CallsRelayUseFallbackServer, fallbackCallRelayServerEnabledChanged, Value);
    CONNECT_SETTING_ID(CallsDevicesMicrophone, microphoneChanged, Value, Values);
    CONNECT_SETTING_ID(CallsDevicesCamera, cameraChanged, Value, Values);
    CONNECT_SETTING_ID(CallsDevicesCameraResolution, cameraResolutionChanged, Value, Values);
    CONNECT_SETTING_ID(CallsDevicesCameraFrameRate, cameraFrameRateChanged, Value, Values);
    CONNECT_SETTING_ID(CallsAudioRingtone, ringtoneChanged, Values, Value);
    CONNECT_SETTING_ID(CallsScreenshareFrameRate, screenShareFrameRateChanged, Value);
    CONNECT_SETTING_ID(CallsScreensharePictureInPicture, screenSharePiPChanged, Value);
    CONNECT_SETTING_ID(CallsScreenshareIncludeRemoteVideo, screenShareRemoteVideoChanged, Value);
    CONNECT_SETTING_ID(CallsScreenshareShowCursor, screenShareShowCursorChanged, Value);
