    // ── Privacy Tab ─────────────────────────────────────────────────────────

    // PrivacyScreenLockSection
    { QT_TR_NOOP("Screen lock"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PrivacyScreen
    SIMPLE_BOOL_CONFIG_ID_SETTING("Privacy screen",
                        QT_TR_NOOP("Blur the timeline when the app loses window focus."),
                        SM::TabPrivacy, privacyScreen, setPrivacyScreen, nullptr, PrivacyScreenLockEnabled, SettingKey::PrivacyScreenLockEnabled),
    // PrivacyScreenTimeoutSeconds
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(
      "Privacy screen timeout (seconds)",
      QT_TR_NOOP(
        "Set how long after focus loss before blurring starts. Set 0 to blur immediately. Maximum is 3600 seconds (1 hour)."),
      SM::TabPrivacy, privacyScreenTimeoutSeconds, setPrivacyScreenTimeoutSeconds, 0, 3600, 10,
      nullptr, privacyScreenTimeoutEnabled, PrivacyScreenLockTimeoutSeconds, SettingKey::PrivacyScreenLockTimeoutSeconds),
    // PrivacyDataSection
    { QT_TR_NOOP("Data & maintenance"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ExpireEvents
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Periodically delete expired events",
      QT_TR_NOOP("Regularly redact expired events based on room expiration rules. Because homeservers may not do this automatically, at least one of your clients should keep this enabled."),
      SM::TabPrivacy, expireEvents, setExpireEvents, nullptr, PrivacyMaintenanceExpireEvents, SettingKey::PrivacyMaintenanceExpireEvents),
    // HiddenTimelineEvents
    { QT_TR_NOOP("Hidden events"),
      QT_TR_NOOP("Configure whether to show or hide certain events like room joins."),
      SM::ConfigureHiddenEvents, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UpdateSpaceVias
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Periodically update community routing information",
      QT_TR_NOOP("Keep community routing metadata up to date in the background so joins continue to work as room membership changes."),
      SM::TabPrivacy, updateSpaceVias, setUpdateSpaceVias, nullptr, PrivacyMaintenanceUpdateSpaceVias, SettingKey::PrivacyMaintenanceUpdateSpaceVias),
    // PrivacyUsersSection
    { QT_TR_NOOP("Users"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IgnoredUsers
    { QT_TR_NOOP("Ignored users"),
      QT_TR_NOOP("Manage your ignored users."),
      SM::ManageIgnoredUsers, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
