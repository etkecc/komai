    // ── Account Tab ─────────────────────────────────────────────────────────

    // AccountSection
    { QT_TR_NOOP("Account"), nullptr, SM::SectionTitle, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UserId
    { QT_TR_NOOP("User ID"), nullptr, SM::ReadOnlyText, SM::TabAccount,
      getSettingValue<&UserSettings::userId>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Homeserver
    { QT_TR_NOOP("Homeserver"), nullptr, SM::ReadOnlyText, SM::TabAccount,
      getSettingValue<&UserSettings::homeserver>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // Profile
    { QT_TR_NOOP("Edit profile"), nullptr, SM::ProfileButton, SM::TabAccount,
      sessionProfileValue,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccountDeviceSection
    { QT_TR_NOOP("Device"), nullptr, SM::SectionTitle, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // DeviceId
    { QT_TR_NOOP("Device ID"), nullptr, SM::ReadOnlyText, SM::TabAccount,
      getSettingValue<&UserSettings::deviceId>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccessToken
    { QT_TR_NOOP("Access token"),
      QT_TR_NOOP("Your access token gives full access to your account. Do not share it with anyone."),
      SM::AccessTokenField, SM::TabAccount,
      getSettingValue<&UserSettings::accessToken>,
      nullptr, {}, {}, {}, nullptr, nullptr },
    // AccountUsersSection
    { QT_TR_NOOP("Users"), nullptr, SM::SectionTitle, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IgnoredUsers
    { QT_TR_NOOP("Ignored users"),
      nullptr,
      SM::ManageIgnoredUsers, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr,
      settings::core::SettingId::Unknown,
      nullptr,  // icon
      nullptr,  // tagId
      true },   // syncedToMatrix
    // AccountActionsSection
    { QT_TR_NOOP("Actions"), nullptr, SM::SectionTitle, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Logout
    { QT_TR_NOOP("Log out"), nullptr, SM::LogoutButton, SM::TabAccount,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
