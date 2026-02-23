    // ── Privacy Tab ─────────────────────────────────────────────────────────

    // PrivacyPresenceSection
    { QT_TR_NOOP("Presence"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PresenceDefault
    { QT_TR_NOOP("Status policy"),
      QT_TR_NOOP("Controls your <a href=\"%1\">Presence</a> status on the Matrix network.\nAutomatic is either 'online' or 'unavailable' (after 5 minutes of inactivity)."),
      SM::OptionsWithDescription, SM::TabPrivacy,
      privacyPresenceStatusPolicyValue,
      setPrivacyPresenceStatusPolicyValue,
      {}, {}, {},
      privacyPresenceStatusPolicyValues,
      nullptr,
      CORE_CONFIG_ID(NetworkPresenceStatusPolicy, SettingKey::NetworkPresenceStatusPolicy) },
    // PresenceStatusMessage
    { QT_TR_NOOP("Status message"),
      nullptr,
      SM::PresenceStatusMessageField, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

    // PrivacyScreenLockSection
    { QT_TR_NOOP("Screen lock"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PrivacyScreen
    SIMPLE_BOOL_CONFIG_ID_SETTING("Privacy screen",
                        QT_TR_NOOP("When the window loses focus, the timeline will\nbe blurred."),
                        SM::TabPrivacy, privacyScreen, setPrivacyScreen, nullptr, PrivacyScreenLockEnabled, SettingKey::PrivacyScreenLockEnabled),
    // PrivacyScreenTimeoutSeconds
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING(
      "Privacy screen timeout (seconds)",
      QT_TR_NOOP(
        "Set how long after focus loss before the screen is blurred.\nSet to 0 to blur immediately after focus loss.\nMaximum is 3600 seconds (1 hour)."),
      SM::TabPrivacy, privacyScreenTimeoutSeconds, setPrivacyScreenTimeoutSeconds, 0, 3600, 10,
      nullptr, privacyScreenTimeoutEnabled, PrivacyScreenLockTimeoutSeconds, SettingKey::PrivacyScreenLockTimeoutSeconds),
    // PrivacyDataSection
    { QT_TR_NOOP("Data & maintenance"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ExpireEvents
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Periodically delete expired events",
      QT_TR_NOOP("Regularly redact expired events as specified in the event expiration configuration. Since this is currently not executed server side, you need to have one client running this regularly."),
      SM::TabPrivacy, expireEvents, setExpireEvents, nullptr, PrivacyMaintenanceExpireEvents, SettingKey::PrivacyMaintenanceExpireEvents),
    // HiddenTimelineEvents
    { QT_TR_NOOP("Hidden events"),
      QT_TR_NOOP("Configure whether to show or hide certain events like room joins."),
      SM::ConfigureHiddenEvents, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UpdateSpaceVias
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Periodically update community routing information",
      QT_TR_NOOP("To allow new users to join a community, the community needs to expose some information about what servers participate in a room to community members. Since the room participants can change over time, this needs to be updated from time to time. This setting enables a background job to do that automatically."),
      SM::TabPrivacy, updateSpaceVias, setUpdateSpaceVias, nullptr, PrivacyMaintenanceUpdateSpaceVias, SettingKey::PrivacyMaintenanceUpdateSpaceVias),
    // PrivacyUsersSection
    { QT_TR_NOOP("Users"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IgnoredUsers
    { QT_TR_NOOP("Ignored users"),
      QT_TR_NOOP("Manage your ignored users."),
      SM::ManageIgnoredUsers, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
