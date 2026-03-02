    // ── Calls Tab ───────────────────────────────────────────────────────────

    // CallsGeneralSection
    { QT_TR_NOOP("General"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableLegacyCalls
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable legacy calls",
      QT_TR_NOOP("Allows legacy calls (potentially unreliable & not supported everywhere) to be accepted and initiated."),
      SM::TabCalls, callsLegacyEnabled, setCallsLegacyEnabled, nullptr, CallsLegacyEnabled, SettingKey::CallsLegacyEnabled),
    // UseFallbackCallRelayServer
    SIMPLE_BOOL_CONFIG_ID_SETTING("Use turn.matrix.org as fallback relay",
                        QT_TR_NOOP("Use turn.matrix.org as a fallback relay/STUN server when your homeserver does not provide one."),
                        SM::TabCalls, callsRelayUseFallbackServer, setCallsRelayUseFallbackServer, nullptr, CallsRelayUseFallbackServer, SettingKey::CallsRelayUseFallbackServer),
    // CallsDevicesSection
    { QT_TR_NOOP("Devices"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Microphone
    { QT_TR_NOOP("Microphone"),
      nullptr,
      SM::Options, SM::TabCalls,
      callMicrophoneValue,
      setCallMicrophoneValue,
      {}, {}, {},
      callMicrophoneValues,
      nullptr,
      CORE_CONFIG_ID(CallsDevicesMicrophone, SettingKey::CallsDevicesMicrophone) },
    // Camera
    { QT_TR_NOOP("Camera"), nullptr, SM::Options, SM::TabCalls,
      callCameraValue,
      setCallCameraValue,
      {}, {}, {},
      callCameraValues,
      nullptr,
      CORE_CONFIG_ID(CallsDevicesCamera, SettingKey::CallsDevicesCamera) },
    // CameraResolution
    { QT_TR_NOOP("Camera resolution"), nullptr, SM::Options, SM::TabCalls,
      callCameraResolutionValue,
      setCallCameraResolutionValue,
      {}, {}, {},
      callCameraResolutionValues,
      nullptr,
      CORE_CONFIG_ID(CallsDevicesCameraResolution, SettingKey::CallsDevicesCameraResolution) },
    // CameraFrameRate
    { QT_TR_NOOP("Camera frame rate"), nullptr, SM::Options, SM::TabCalls,
      callCameraFrameRateValue,
      setCallCameraFrameRateValue,
      {}, {}, {},
      callCameraFrameRateValues,
      nullptr,
      CORE_CONFIG_ID(CallsDevicesCameraFrameRate, SettingKey::CallsDevicesCameraFrameRate) },
    // Ringtone
    { QT_TR_NOOP("Ringtone"), nullptr, SM::Options, SM::TabCalls,
      callRingtoneValue,
      setCallRingtoneValue,
      {}, {}, {},
      callRingtoneValues,
      nullptr,
      CORE_CONFIG_ID(CallsAudioRingtone, SettingKey::CallsAudioRingtone) },

    // CallsScreenSharingSection
    { QT_TR_NOOP("Screen sharing"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CallsScreenshareFrameRate
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(
      "Screen share frame rate",
      QT_TR_NOOP("Target frames per second for screen sharing. Lower values use less CPU and bandwidth."),
      SM::TabCalls, callsScreenshareFrameRate, setCallsScreenshareFrameRate, 1, 120, 1,
      nullptr, nullptr, CallsScreenshareFrameRate, SettingKey::CallsScreenshareFrameRate),
    // CallsScreensharePictureInPicture
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Include camera picture-in-picture",
      nullptr,
      SM::TabCalls, callsScreensharePictureInPicture, setCallsScreensharePictureInPicture, nullptr, CallsScreensharePictureInPicture, SettingKey::CallsScreensharePictureInPicture),
    // CallsScreenshareIncludeRemoteVideo
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show participant camera while screen sharing",
      nullptr,
      SM::TabCalls, callsScreenshareIncludeRemoteVideo, setCallsScreenshareIncludeRemoteVideo, nullptr, CallsScreenshareIncludeRemoteVideo, SettingKey::CallsScreenshareIncludeRemoteVideo),
    // CallsScreenshareShowCursor
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show mouse cursor",
      nullptr,
      SM::TabCalls, callsScreenshareShowCursor, setCallsScreenshareShowCursor, nullptr, CallsScreenshareShowCursor, SettingKey::CallsScreenshareShowCursor),
