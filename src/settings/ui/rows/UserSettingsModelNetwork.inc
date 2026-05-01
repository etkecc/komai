    // ── Network Tab ─────────────────────────────────────────────────────────

    // NetworkEncryptionSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Encryption"), nullptr, SM::SectionTitle, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EncryptionKeySharingOnlyVerifiedUsers
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Send encrypted messages to verified users only"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Requires a user to be verified to send encrypted messages to them. This improves safety but makes E2EE more tedious."),
      SM::TabNetwork, encryptionKeySharingOnlyVerifiedUsers, setEncryptionKeySharingOnlyVerifiedUsers, nullptr, EncryptionKeySharingOnlyVerifiedUsers, SettingKey::EncryptionKeySharingOnlyVerifiedUsers),
    // EncryptionKeySharingShareWithTrusted
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Share keys with verified users and devices"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Automatically replies to key requests from other users if they are verified, even if that device shouldn't have access to those keys otherwise."),
      SM::TabNetwork, encryptionKeySharingShareWithTrusted, setEncryptionKeySharingShareWithTrusted, nullptr, EncryptionKeySharingShareWithTrusted, SettingKey::EncryptionKeySharingShareWithTrusted),
    // EncryptionBackupOnlineEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Enable online key backup"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Download message encryption keys from and upload to the encrypted online key backup."),
      SM::TabNetwork, encryptionBackupOnlineEnabled, setEncryptionBackupOnlineEnabled, nullptr, EncryptionBackupOnlineEnabled, SettingKey::EncryptionBackupOnlineEnabled),

    // NetworkPresenceSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Presence"), nullptr, SM::SectionTitle, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // PresenceDefault
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Status policy"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Controls your <a href=\"%1\">Presence</a> status on the Matrix network.\nAutomatic is either 'online' or 'unavailable' (after 5 minutes of inactivity)."),
      SM::OptionsWithDescription, SM::TabNetwork,
      networkPresenceStatusPolicyValue,
      setNetworkPresenceStatusPolicyValue,
      {}, {}, {},
      networkPresenceStatusPolicyValues,
      nullptr,
      CORE_CONFIG_ID(NetworkPresenceStatusPolicy, SettingKey::NetworkPresenceStatusPolicy) },
    // PresenceStatusMessage
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Status message"),
      nullptr,
      SM::PresenceStatusMessageField, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr,
      settings::core::SettingId::Unknown,
      nullptr,  // icon
      nullptr,  // tagId
      true },   // syncedToMatrix

    // NetworkTransportSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Transport"), nullptr, SM::SectionTitle, SM::TabNetwork,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // NetworkTlsEnableCertificateValidation
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Validate TLS certificates"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Reject invalid or untrusted server certificates."),
      SM::TabNetwork, networkTlsEnableCertificateValidation, setNetworkTlsEnableCertificateValidation, nullptr, NetworkTlsEnableCertificateValidation, SettingKey::NetworkTlsEnableCertificateValidation),
    // NetworkHttp3Enabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Enable HTTP/3"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Use HTTP/3 for Matrix server connections when available."),
      SM::TabNetwork, networkHttp3Enabled, setNetworkHttp3Enabled, nullptr, NetworkHttp3Enabled, SettingKey::NetworkHttp3Enabled),
