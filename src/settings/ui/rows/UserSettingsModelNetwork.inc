    // ── Network Tab ─────────────────────────────────────────────────────────

    // NetworkPresenceSection
    { QT_TR_NOOP("Presence"), nullptr, SM::SectionTitle, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PresenceDefault
    { QT_TR_NOOP("Status policy"),
      QT_TR_NOOP("Controls your <a href=\"%1\">Presence</a> status on the Matrix network.\nAutomatic is either 'online' or 'unavailable' (after 5 minutes of inactivity)."),
      SM::OptionsWithDescription, SM::TabNetwork,
      networkPresenceStatusPolicyValue,
      setNetworkPresenceStatusPolicyValue,
      {}, {}, {},
      networkPresenceStatusPolicyValues,
      nullptr,
      CORE_CONFIG_ID(NetworkPresenceStatusPolicy, SettingKey::NetworkPresenceStatusPolicy) },
    // PresenceStatusMessage
    { QT_TR_NOOP("Status message"),
      nullptr,
      SM::PresenceStatusMessageField, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr,
      settings::core::SettingId::Unknown,
      nullptr,  // icon
      nullptr,  // tagId
      true },   // syncedToMatrix

    // NetworkSpacesSection
    { QT_TR_NOOP("Spaces"), nullptr, SM::SectionTitle, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // NetworkSpacesMaintainJoinMetadata
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Maintain space join metadata",
      QT_TR_NOOP("Recomputes & updates `via` servers for rooms linked from spaces."),
      SM::TabNetwork, networkSpacesMaintainJoinMetadata, setNetworkSpacesMaintainJoinMetadata, nullptr, NetworkSpacesMaintainJoinMetadata, SettingKey::NetworkSpacesMaintainJoinMetadata),

    // NetworkTransportSection
    { QT_TR_NOOP("Transport"), nullptr, SM::SectionTitle, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // NetworkTlsEnableCertificateValidation
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Validate TLS certificates",
      QT_TR_NOOP("Reject invalid or untrusted server certificates."),
      SM::TabNetwork, networkTlsEnableCertificateValidation, setNetworkTlsEnableCertificateValidation, nullptr, NetworkTlsEnableCertificateValidation, SettingKey::NetworkTlsEnableCertificateValidation),
    // NetworkHttp3Enabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable HTTP/3",
      QT_TR_NOOP("Use HTTP/3 for Matrix server connections when available."),
      SM::TabNetwork, networkHttp3Enabled, setNetworkHttp3Enabled, nullptr, NetworkHttp3Enabled, SettingKey::NetworkHttp3Enabled),
