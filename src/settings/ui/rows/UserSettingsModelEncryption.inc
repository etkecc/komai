    // ── Encryption Tab ──────────────────────────────────────────────────────

    // EncryptionKeySharingSection
    { QT_TR_NOOP("Key sharing"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EncryptionKeySharingOnlyVerifiedUsers
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Send encrypted messages to verified users only",
      QT_TR_NOOP("Requires a user to be verified to send encrypted messages to them. This improves safety but makes E2EE more tedious."),
      SM::TabEncryption, encryptionKeySharingOnlyVerifiedUsers, setEncryptionKeySharingOnlyVerifiedUsers, nullptr, EncryptionKeySharingOnlyVerifiedUsers, SettingKey::EncryptionKeySharingOnlyVerifiedUsers),
    // EncryptionKeySharingShareWithTrusted
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Share keys with verified users and devices",
      QT_TR_NOOP("Automatically replies to key requests from other users if they are verified, even if that device shouldn't have access to those keys otherwise."),
      SM::TabEncryption, encryptionKeySharingShareWithTrusted, setEncryptionKeySharingShareWithTrusted, nullptr, EncryptionKeySharingShareWithTrusted, SettingKey::EncryptionKeySharingShareWithTrusted),
    // EncryptionBackupSection
    { QT_TR_NOOP("Backup"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EncryptionBackupOnlineEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable online key backup",
      QT_TR_NOOP("Download message encryption keys from and upload to the encrypted online key backup."),
      SM::TabEncryption, encryptionBackupOnlineEnabled, setEncryptionBackupOnlineEnabled, nullptr, EncryptionBackupOnlineEnabled, SettingKey::EncryptionBackupOnlineEnabled),
