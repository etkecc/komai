    // Calls
    CONNECT_SETTING_KEY(SettingKey::CallsLegacyEnabled, enableLegacyCallsChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::CallsRelayUseFallbackServer, useFallbackCallRelayServerChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::CallsDevicesMicrophone, microphoneChanged, Value, Values);
    CONNECT_SETTING_KEY(SettingKey::CallsDevicesCamera, cameraChanged, Value, Values);
    CONNECT_SETTING_KEY(SettingKey::CallsDevicesCameraResolution, cameraResolutionChanged, Value, Values);
    CONNECT_SETTING_KEY(SettingKey::CallsDevicesCameraFrameRate, cameraFrameRateChanged, Value, Values);
    CONNECT_SETTING_KEY(SettingKey::CallsAudioRingtone, ringtoneChanged, Values, Value);
