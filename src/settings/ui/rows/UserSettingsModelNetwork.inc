    // ── Network Tab ─────────────────────────────────────────────────────────

    // NetworkPresenceSection
    { QT_TR_NOOP("Presence"), nullptr, SM::SectionTitle, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PresenceDefault
    { QT_TR_NOOP("Status policy"),
      QT_TR_NOOP("Controls your <a href=\"%1\">Presence</a> status on the Matrix network.\nAutomatic is either 'online' or 'unavailable' (after 5 minutes of inactivity)."),
      SM::OptionsWithDescription, SM::TabNetwork,
      privacyPresenceStatusPolicyValue,
      setPrivacyPresenceStatusPolicyValue,
      {}, {}, {},
      privacyPresenceStatusPolicyValues,
      nullptr,
      CORE_CONFIG_ID(NetworkPresenceStatusPolicy, SettingKey::NetworkPresenceStatusPolicy) },
    // PresenceStatusMessage
    { QT_TR_NOOP("Status message"),
      nullptr,
      SM::PresenceStatusMessageField, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
