    // ── Calls Tab ───────────────────────────────────────────────────────────

    // CallsGeneralSection
    { QT_TR_NOOP("General"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableLegacyCalls
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable legacy calls",
      QT_TR_NOOP("Show the call button in the message composer. This uses the old VoIP calling feature which may not work reliably. Element Call support is expected in a future release."),
      SM::TabCalls, legacyCallsEnabled, setLegacyCallsEnabled, nullptr, CallsLegacyEnabled, SettingKey::CallsLegacyEnabled),
    // UseFallbackCallRelayServer
    SIMPLE_BOOL_CONFIG_ID_SETTING("Use fallback call relay server",
                        QT_TR_NOOP("Use turn.matrix.org as a fallback relay/STUN server when your homeserver does not provide one."),
                        SM::TabCalls, useFallbackCallRelayServer, setUseFallbackCallRelayServer, nullptr, CallsRelayUseFallbackServer, SettingKey::CallsRelayUseFallbackServer),
    // CallsDevicesSection
    { QT_TR_NOOP("Devices"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Microphone
    { QT_TR_NOOP("Microphone"),
      QT_TR_NOOP("Select the microphone used for voice and video calls."),
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
