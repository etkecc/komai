    // Calls
    CONNECT_SETTING(EnableLegacyCalls, enableLegacyCallsChanged, Value);
    CONNECT_SETTING(UseFallbackCallRelayServer, useFallbackCallRelayServerChanged, Value);
    CONNECT_SETTING(Microphone, microphoneChanged, Value, Values);
    CONNECT_SETTING(Camera, cameraChanged, Value, Values);
    CONNECT_SETTING(CameraResolution, cameraResolutionChanged, Value, Values);
    CONNECT_SETTING(CameraFrameRate, cameraFrameRateChanged, Value, Values);
    CONNECT_SETTING(Ringtone, ringtoneChanged, Values, Value);
