    // ── Privacy Tab ─────────────────────────────────────────────────────────

    // PrivacyWindowFocusBlurSection
    { QT_TR_NOOP("Window blur"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PrivacyWindowFocusBlur
    SIMPLE_BOOL_CONFIG_ID_SETTING("Blur on focus loss",
                        nullptr,
                        SM::TabPrivacy, privacyWindowFocusBlurEnabled, setPrivacyWindowFocusBlurEnabled, nullptr, PrivacyWindowFocusBlurEnabled, SettingKey::PrivacyWindowFocusBlurEnabled),
    // PrivacyWindowFocusBlurDelaySeconds
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(
      "Blur delay (seconds)",
      QT_TR_NOOP(
        "Set how long after focus loss before blurring starts. Set 0 to blur immediately. Maximum is 3600 seconds (1 hour)."),
      SM::TabPrivacy, privacyWindowFocusBlurDelaySeconds, setPrivacyWindowFocusBlurDelaySeconds, 0, 3600, 10,
      nullptr, privacyWindowFocusBlurDelaySecondsEnabled, PrivacyWindowFocusBlurDelaySeconds, SettingKey::PrivacyWindowFocusBlurDelaySeconds),
    // PrivacyDataSection
    { QT_TR_NOOP("Data & maintenance"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ExpireEvents
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Delete expired events periodically",
      QT_TR_NOOP("Regularly redact expired events based on room expiration rules. Because homeservers may not do this automatically, at least one of your clients should keep this enabled."),
      SM::TabPrivacy, privacyMaintenanceExpireEvents, setPrivacyMaintenanceExpireEvents, nullptr, PrivacyMaintenanceExpireEvents, SettingKey::PrivacyMaintenanceExpireEvents),
    // HiddenTimelineEvents
    { QT_TR_NOOP("Hidden events"),
      QT_TR_NOOP("Configure whether to show or hide certain events like room joins."),
      SM::ConfigureHiddenEvents, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UpdateSpaceVias
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Update community routing info periodically",
      QT_TR_NOOP("Keep community routing metadata up to date in the background so joins continue to work as room membership changes."),
      SM::TabPrivacy, privacyMaintenanceUpdateSpaceVias, setPrivacyMaintenanceUpdateSpaceVias, nullptr, PrivacyMaintenanceUpdateSpaceVias, SettingKey::PrivacyMaintenanceUpdateSpaceVias),
    // PrivacyUsersSection
    { QT_TR_NOOP("Users"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IgnoredUsers
    { QT_TR_NOOP("Ignored users"),
      nullptr,
      SM::ManageIgnoredUsers, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
