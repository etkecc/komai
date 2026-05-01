    // ── Account Tab ─────────────────────────────────────────────────────────

    // AccountSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Account"), nullptr, SM::SectionTitle, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UserId
    { QT_TRANSLATE_NOOP("UserSettingsModel", "User ID"), nullptr, SM::ReadOnlyText, SM::TabAccount,
      getSettingValue<&UserSettings::userId>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Homeserver
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Homeserver"), nullptr, SM::ReadOnlyText, SM::TabAccount,
      getSettingValue<&UserSettings::homeserver>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Profile
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Edit profile"), nullptr, SM::ProfileButton, SM::TabAccount,
      sessionProfileValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccountDeviceSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Device"), nullptr, SM::SectionTitle, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DeviceId
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Device ID"), nullptr, SM::ReadOnlyText, SM::TabAccount,
      getSettingValue<&UserSettings::deviceId>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccessToken
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Access token"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Your access token gives full access to your account. Do not share it with anyone."),
      SM::AccessTokenField, SM::TabAccount,
      getSettingValue<&UserSettings::accessToken>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccountUsersSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Users"), nullptr, SM::SectionTitle, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IgnoredUsers
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Ignored users"),
      nullptr,
      SM::ManageIgnoredUsers, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr,
      settings::core::SettingId::Unknown,
      nullptr,  // icon
      nullptr,  // tagId
      true },   // syncedToMatrix
    // AccountActionsSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Actions"), nullptr, SM::SectionTitle, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Logout
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Log out"), nullptr, SM::LogoutButton, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
