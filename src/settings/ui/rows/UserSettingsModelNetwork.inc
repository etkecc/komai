    // ── Network Tab ─────────────────────────────────────────────────────────

    // NetworkEncryptionSection
    { QT_TR_NOOP("Encryption"), nullptr, SM::SectionTitle, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EncryptionKeySharingOnlyVerifiedUsers
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Send encrypted messages to verified users only",
      QT_TR_NOOP("Requires a user to be verified to send encrypted messages to them. This improves safety but makes E2EE more tedious."),
      SM::TabNetwork, encryptionKeySharingOnlyVerifiedUsers, setEncryptionKeySharingOnlyVerifiedUsers, nullptr, EncryptionKeySharingOnlyVerifiedUsers, SettingKey::EncryptionKeySharingOnlyVerifiedUsers),
    // EncryptionKeySharingShareWithTrusted
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Share keys with verified users and devices",
      QT_TR_NOOP("Automatically replies to key requests from other users if they are verified, even if that device shouldn't have access to those keys otherwise."),
      SM::TabNetwork, encryptionKeySharingShareWithTrusted, setEncryptionKeySharingShareWithTrusted, nullptr, EncryptionKeySharingShareWithTrusted, SettingKey::EncryptionKeySharingShareWithTrusted),
    // EncryptionBackupOnlineEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable online key backup",
      QT_TR_NOOP("Download message encryption keys from and upload to the encrypted online key backup."),
      SM::TabNetwork, encryptionBackupOnlineEnabled, setEncryptionBackupOnlineEnabled, nullptr, EncryptionBackupOnlineEnabled, SettingKey::EncryptionBackupOnlineEnabled),

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
