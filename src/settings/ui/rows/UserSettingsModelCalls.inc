    // ── Calls Tab ───────────────────────────────────────────────────────────

    // CallsGeneralSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "General"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableLegacyCalls
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Enable legacy calls"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Allows legacy (pre-MatrixRTC) calls to be accepted and initiated."),
      SM::TabCalls, callsLegacyEnabled, setCallsLegacyEnabled, nullptr, CallsLegacyEnabled, SettingKey::CallsLegacyEnabled),
    // UseFallbackCallRelayServer
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Use turn.matrix.org as fallback relay"),
                        QT_TRANSLATE_NOOP("UserSettingsModel", "Use turn.matrix.org as a fallback relay/STUN server when your homeserver does not provide one."),
                        SM::TabCalls, callsRelayUseFallbackServer, setCallsRelayUseFallbackServer, nullptr, CallsRelayUseFallbackServer, SettingKey::CallsRelayUseFallbackServer),
    // CallsDevicesSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Devices"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Microphone
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Microphone"),
      nullptr,
      SM::Options, SM::TabCalls,
      callMicrophoneValue,
      setCallMicrophoneValue,
      {}, {}, {},
      callMicrophoneValues,
      nullptr,
      CORE_CONFIG_ID(CallsDevicesMicrophone, SettingKey::CallsDevicesMicrophone),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "audio input, mic") },
    // Camera
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Camera"), nullptr, SM::Options, SM::TabCalls,
      callCameraValue,
      setCallCameraValue,
      {}, {}, {},
      callCameraValues,
      nullptr,
      CORE_CONFIG_ID(CallsDevicesCamera, SettingKey::CallsDevicesCamera),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "webcam, video input") },
    // CameraResolution
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Camera resolution"), nullptr, SM::Options, SM::TabCalls,
      callCameraResolutionValue,
      setCallCameraResolutionValue,
      {}, {}, {},
      callCameraResolutionValues,
      nullptr,
      CORE_CONFIG_ID(CallsDevicesCameraResolution, SettingKey::CallsDevicesCameraResolution),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "video quality, video size") },
    // CameraFrameRate
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Camera frame rate"), nullptr, SM::Options, SM::TabCalls,
      callCameraFrameRateValue,
      setCallCameraFrameRateValue,
      {}, {}, {},
      callCameraFrameRateValues,
      nullptr,
      CORE_CONFIG_ID(CallsDevicesCameraFrameRate, SettingKey::CallsDevicesCameraFrameRate),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "fps, frames per second, smoothness") },
    // Ringtone
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Ringtone"), nullptr, SM::Options, SM::TabCalls,
      callRingtoneValue,
      setCallRingtoneValue,
      {}, {}, {},
      callRingtoneValues,
      nullptr,
      CORE_CONFIG_ID(CallsAudioRingtone, SettingKey::CallsAudioRingtone),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "call sound, incoming call") },

    // CallsScreenSharingSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Screen sharing"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CallsScreenshareFrameRate
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Screen share frame rate"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Target frames per second for screen sharing. Lower values use less CPU and bandwidth."),
      SM::TabCalls, callsScreenshareFrameRate, setCallsScreenshareFrameRate, 1, 120, 1,
      nullptr, nullptr, CallsScreenshareFrameRate, SettingKey::CallsScreenshareFrameRate),
    // CallsScreensharePictureInPicture
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Include camera picture-in-picture"),
      nullptr,
      SM::TabCalls, callsScreensharePictureInPicture, setCallsScreensharePictureInPicture, nullptr, CallsScreensharePictureInPicture, SettingKey::CallsScreensharePictureInPicture),
    // CallsScreenshareIncludeRemoteVideo
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show participant camera while screen sharing"),
      nullptr,
      SM::TabCalls, callsScreenshareIncludeRemoteVideo, setCallsScreenshareIncludeRemoteVideo, nullptr, CallsScreenshareIncludeRemoteVideo, SettingKey::CallsScreenshareIncludeRemoteVideo),
    // CallsScreenshareShowCursor
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show mouse cursor"),
      nullptr,
      SM::TabCalls, callsScreenshareShowCursor, setCallsScreenshareShowCursor, nullptr, CallsScreenshareShowCursor, SettingKey::CallsScreenshareShowCursor),
