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

    // NetworkTransportSection
    { QT_TR_NOOP("Transport"), nullptr, SM::SectionTitle, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // NetworkTlsEnableCertificateValidation
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Validate TLS certificates",
      QT_TR_NOOP("Reject invalid or untrusted server certificates."),
      SM::TabNetwork, certificateValidationEnabled, setCertificateValidationEnabled, nullptr, NetworkTlsEnableCertificateValidation, SettingKey::NetworkTlsEnableCertificateValidation),
    // NetworkHttp3Enabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable HTTP/3",
      QT_TR_NOOP("Use HTTP/3 for Matrix server connections when available."),
      SM::TabNetwork, http3Enabled, setHttp3Enabled, nullptr, NetworkHttp3Enabled, SettingKey::NetworkHttp3Enabled),
