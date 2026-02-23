    // ── Privacy Tab ─────────────────────────────────────────────────────────

    // PrivacyPresenceSection
    { nullptr, nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PresenceDefault
    { QT_TR_NOOP("Status policy"),
      QT_TR_NOOP("Controls your <a href=\"%1\">Presence</a> status on the Matrix network.\nAutomatic is either 'online' or 'unavailable' (after 5 minutes of inactivity)."),
      SM::OptionsWithDescription, SM::TabPrivacy,
      []() -> QVariant {
          switch (I->presence()) {
          case UserSettings::Presence::Online:
              return 0;
          case UserSettings::Presence::Unavailable:
              return 1;
          case UserSettings::Presence::Offline:
              return 2;
          case UserSettings::Presence::AutomaticPresence:
              return 3;
          }
          return 3;
      },
      [](const QVariant &v) -> bool {
          int option = 0;
          if (!readSettingValue(v, option))
              return false;

          switch (option) {
          case 0:
              I->setPresence(UserSettings::Presence::Online);
              return true;
          case 1:
              I->setPresence(UserSettings::Presence::Unavailable);
              return true;
          case 2:
              I->setPresence(UserSettings::Presence::Offline);
              return true;
          case 3:
              I->setPresence(UserSettings::Presence::AutomaticPresence);
              return true;
          default:
              return false;
          }
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always online"),
            QCoreApplication::translate("UserSettingsModel", "Always unavailable"),
            QCoreApplication::translate("UserSettingsModel", "Always offline"),
            QCoreApplication::translate("UserSettingsModel", "Automatic"),
          };
      },
      nullptr },
    // PresenceStatusMessage
    { QT_TR_NOOP("Status message"),
      nullptr,
      SM::PresenceStatusMessageField, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

    // PrivacyScreenLockSection
    { nullptr, nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PrivacyScreen
    SIMPLE_BOOL_SETTING("Privacy screen",
                        QT_TR_NOOP("When the window loses focus, the timeline will\nbe blurred."),
                        SM::TabPrivacy, privacyScreen, setPrivacyScreen, nullptr),
    // PrivacyScreenTimeoutSeconds
    SIMPLE_OPTIONS_INT_SETTING(
      "Privacy screen timeout (seconds)",
      QT_TR_NOOP(
        "Set how long after focus loss before the screen is blurred.\nSet to 0 to blur immediately after focus loss.\nMaximum is 3600 seconds (1 hour)."),
      SM::TabPrivacy, privacyScreenTimeoutSeconds, setPrivacyScreenTimeoutSeconds, 0, 3600, 10,
      nullptr, []() -> bool { return I->privacyScreen(); }),
    // PrivacyDataSection
    { nullptr, nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ExpireEvents
    SIMPLE_BOOL_SETTING(
      "Periodically delete expired events",
      QT_TR_NOOP("Regularly redact expired events as specified in the event expiration configuration. Since this is currently not executed server side, you need to have one client running this regularly."),
      SM::TabPrivacy, expireEvents, setExpireEvents, nullptr),
    // HiddenTimelineEvents
    { QT_TR_NOOP("Hidden events"),
      QT_TR_NOOP("Configure whether to show or hide certain events like room joins."),
      SM::ConfigureHiddenEvents, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UpdateSpaceVias
    SIMPLE_BOOL_SETTING(
      "Periodically update community routing information",
      QT_TR_NOOP("To allow new users to join a community, the community needs to expose some information about what servers participate in a room to community members. Since the room participants can change over time, this needs to be updated from time to time. This setting enables a background job to do that automatically."),
      SM::TabPrivacy, updateSpaceVias, setUpdateSpaceVias, nullptr),
    // PrivacyUsersSection
    { nullptr, nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IgnoredUsers
    { QT_TR_NOOP("Ignored users"),
      QT_TR_NOOP("Manage your ignored users."),
      SM::ManageIgnoredUsers, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
