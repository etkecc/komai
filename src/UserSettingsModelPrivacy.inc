    // ── Privacy Tab ─────────────────────────────────────────────────────────

    // PrivacyScreenLockSection
    { QT_TR_NOOP("SCREEN LOCK"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PrivacyScreen
    { QT_TR_NOOP("Privacy screen"),
      QT_TR_NOOP("When the window loses focus, the timeline will\nbe blurred."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->privacyScreen(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setPrivacyScreen(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // PrivacyScreenTimeoutSeconds
    { QT_TR_NOOP("Privacy screen timeout (seconds)"),
      QT_TR_NOOP("Set how long after focus loss before the screen is blurred.\nSet to 0 to blur immediately after focus loss.\nMaximum is 3600 seconds (1 hour)."),
      SM::Integer, SM::TabPrivacy,
      []() -> QVariant { return I->privacyScreenTimeoutSeconds(); },
      [](const QVariant &v) -> bool {
          if (!v.canConvert<int>()) return false;
          I->setPrivacyScreenTimeoutSeconds(v.toInt()); return true;
      },
      0, 3600, 10, nullptr,
      []() -> bool { return I->privacyScreen(); } },
    // PrivacyDataSection
    { QT_TR_NOOP("DATA & MAINTENANCE"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ExpireEvents
    { QT_TR_NOOP("Periodically delete expired events"),
      QT_TR_NOOP("Regularly redact expired events as specified in the event expiration configuration. Since this is currently not executed server side, you need to have one client running this regularly."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->expireEvents(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setExpireEvents(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // HiddenTimelineEvents
    { QT_TR_NOOP("Hidden events"),
      QT_TR_NOOP("Configure whether to show or hide certain events like room joins."),
      SM::ConfigureHiddenEvents, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UpdateSpaceVias
    { QT_TR_NOOP("Periodically update community routing information"),
      QT_TR_NOOP("To allow new users to join a community, the community needs to expose some information about what servers participate in a room to community members. Since the room participants can change over time, this needs to be updated from time to time. This setting enables a background job to do that automatically."),
      SM::Toggle, SM::TabPrivacy,
      []() -> QVariant { return I->updateSpaceVias(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUpdateSpaceVias(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // PrivacyUsersSection
    { QT_TR_NOOP("USERS"), nullptr, SM::SectionTitle, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // IgnoredUsers
    { QT_TR_NOOP("Ignored users"),
      QT_TR_NOOP("Manage your ignored users."),
      SM::ManageIgnoredUsers, SM::TabPrivacy,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
