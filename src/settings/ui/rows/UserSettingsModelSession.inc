    // ── Session Tab ─────────────────────────────────────────────────────────

    // SessionAccountSection
    { QT_TR_NOOP("Account"), nullptr, SM::SectionTitle, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UserId
    { QT_TR_NOOP("User ID"), nullptr, SM::ReadOnlyText, SM::TabSession,
      getSettingValue<&UserSettings::userId>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Homeserver
    { QT_TR_NOOP("Homeserver"), nullptr, SM::ReadOnlyText, SM::TabSession,
      getSettingValue<&UserSettings::homeserver>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Profile
    { QT_TR_NOOP("Profile"), nullptr, SM::ProfileButton, SM::TabSession,
      sessionProfileValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // SessionDeviceSection
    { QT_TR_NOOP("Device"), nullptr, SM::SectionTitle, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DeviceId
    { QT_TR_NOOP("Device ID"), nullptr, SM::ReadOnlyText, SM::TabSession,
      getSettingValue<&UserSettings::deviceId>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // DeviceFingerprint
    { QT_TR_NOOP("Device fingerprint"), nullptr, SM::ReadOnlyText, SM::TabSession,
      sessionDeviceFingerprintValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccessToken
    { QT_TR_NOOP("Access token"),
      QT_TR_NOOP("Your access token gives full access to your account. Do not share it with anyone."),
      SM::AccessTokenField, SM::TabSession,
      getSettingValue<&UserSettings::accessToken>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // SessionActionsSection
    { QT_TR_NOOP("Actions"), nullptr, SM::SectionTitle, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Logout
    { QT_TR_NOOP("Logout"), nullptr, SM::LogoutButton, SM::TabSession,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
