    // ── Calls Tab ───────────────────────────────────────────────────────────

    // CallsGeneralSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "General"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableElementCall
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Enable Element Call"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Allows <a href=\"https://github.com/element-hq/element-call\">Element Call</a> (MatrixRTC) group voice and video calls. Requires a homeserver with a MatrixRTC backend."),
      SM::TabCalls, callsElementEnabled, setCallsElementEnabled, nullptr, CallsElementEnabled, SettingKey::CallsElementEnabled),
    // EnableLegacyCalls
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Enable legacy calls"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Allows legacy (pre-MatrixRTC) calls to be accepted and initiated."),
      SM::TabCalls, callsLegacyEnabled, setCallsLegacyEnabled, nullptr, CallsLegacyEnabled, SettingKey::CallsLegacyEnabled),
    // Ringtone — shared by both legacy and Element Call ringers, so it lives in General.
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Ringtone"), nullptr, SM::Options, SM::TabCalls,
      callRingtoneValue,
      setCallRingtoneValue,
      {}, {}, {},
      callRingtoneValues,
      nullptr,
      CORE_CONFIG_ID(CallsAudioRingtone, SettingKey::CallsAudioRingtone),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "call sound, incoming call") },

    // CallsElementSection — no host-side controls today (Element Call manages its own devices and
    // screen sharing inside the in-call interface). The guidance note is rendered as the section's
    // helperText in SettingsContent.qml, keyed on this tagId.
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Element Call configuration"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr,
      settings::core::SettingId::Unknown, nullptr, "calls-element-section" },

    // CallsLegacyDevicesSection — legacy/GStreamer-only; greyed out when legacy calls are off.
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Legacy Calls (Devices)"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Microphone
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Microphone"),
      nullptr,
      SM::Options, SM::TabCalls,
      callMicrophoneValue,
      setCallMicrophoneValue,
      {}, {}, {},
      callMicrophoneValues,
      callsLegacyConfigEnabled,
      CORE_CONFIG_ID(CallsDevicesMicrophone, SettingKey::CallsDevicesMicrophone),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "audio input, mic") },
    // Camera
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Camera"), nullptr, SM::Options, SM::TabCalls,
      callCameraValue,
      setCallCameraValue,
      {}, {}, {},
      callCameraValues,
      callsLegacyConfigEnabled,
      CORE_CONFIG_ID(CallsDevicesCamera, SettingKey::CallsDevicesCamera),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "webcam, video input") },
    // CameraResolution
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Camera resolution"), nullptr, SM::Options, SM::TabCalls,
      callCameraResolutionValue,
      setCallCameraResolutionValue,
      {}, {}, {},
      callCameraResolutionValues,
      callsLegacyConfigEnabled,
      CORE_CONFIG_ID(CallsDevicesCameraResolution, SettingKey::CallsDevicesCameraResolution),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "video quality, video size") },
    // CameraFrameRate
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Camera frame rate"), nullptr, SM::Options, SM::TabCalls,
      callCameraFrameRateValue,
      setCallCameraFrameRateValue,
      {}, {}, {},
      callCameraFrameRateValues,
      callsLegacyConfigEnabled,
      CORE_CONFIG_ID(CallsDevicesCameraFrameRate, SettingKey::CallsDevicesCameraFrameRate),
      nullptr, nullptr, false,
      QT_TRANSLATE_NOOP("UserSettingsModel", "fps, frames per second, smoothness") },
    // UseFallbackCallRelayServer — legacy-only: Element Call gets TURN via the widget driver's
    // granted capability from the homeserver, not this setting.
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Use turn.matrix.org as fallback relay"),
                        QT_TRANSLATE_NOOP("UserSettingsModel", "Use turn.matrix.org as a fallback relay/STUN server when your homeserver does not provide one."),
                        SM::TabCalls, callsRelayUseFallbackServer, setCallsRelayUseFallbackServer, callsLegacyConfigEnabled, CallsRelayUseFallbackServer, SettingKey::CallsRelayUseFallbackServer),

    // CallsLegacyScreenSharingSection — legacy/GStreamer-only; greyed out when legacy calls are off.
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Legacy Calls (Screen Sharing)"), nullptr, SM::SectionTitle, SM::TabCalls,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // CallsScreenshareFrameRate
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Screen share frame rate"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Target frames per second for screen sharing. Lower values use less CPU and bandwidth."),
      SM::TabCalls, callsScreenshareFrameRate, setCallsScreenshareFrameRate, 1, 120, 1,
      nullptr, callsLegacyConfigEnabled, CallsScreenshareFrameRate, SettingKey::CallsScreenshareFrameRate),
    // CallsScreensharePictureInPicture
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Include camera picture-in-picture"),
      nullptr,
      SM::TabCalls, callsScreensharePictureInPicture, setCallsScreensharePictureInPicture, callsLegacyConfigEnabled, CallsScreensharePictureInPicture, SettingKey::CallsScreensharePictureInPicture),
    // CallsScreenshareIncludeRemoteVideo
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show participant camera while screen sharing"),
      nullptr,
      SM::TabCalls, callsScreenshareIncludeRemoteVideo, setCallsScreenshareIncludeRemoteVideo, callsLegacyConfigEnabled, CallsScreenshareIncludeRemoteVideo, SettingKey::CallsScreenshareIncludeRemoteVideo),
    // CallsScreenshareShowCursor
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show mouse cursor"),
      nullptr,
      SM::TabCalls, callsScreenshareShowCursor, setCallsScreenshareShowCursor, callsLegacyConfigEnabled, CallsScreenshareShowCursor, SettingKey::CallsScreenshareShowCursor),
