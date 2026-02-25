    // ── Encryption Tab ──────────────────────────────────────────────────────

    // EncryptionKeySharingSection
    { QT_TR_NOOP("Key sharing"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OnlyShareKeysWithVerifiedUsers
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Send encrypted messages to verified users only",
      QT_TR_NOOP("Requires a user to be verified to send encrypted messages to them. This improves safety but makes E2EE more tedious."),
      SM::TabEncryption, onlyShareKeysWithVerifiedUsers, setOnlyShareKeysWithVerifiedUsers, nullptr, EncryptionKeySharingOnlyVerifiedUsers, SettingKey::EncryptionKeySharingOnlyVerifiedUsers),
    // ShareKeysWithTrustedUsers
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Share keys with verified users and devices",
      QT_TR_NOOP("Automatically replies to key requests from other users if they are verified, even if that device shouldn't have access to those keys otherwise."),
      SM::TabEncryption, shareKeysWithTrustedUsers, setShareKeysWithTrustedUsers, nullptr, EncryptionKeySharingShareWithTrusted, SettingKey::EncryptionKeySharingShareWithTrusted),
    // EncryptionBackupSection
    { QT_TR_NOOP("Backup"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UseOnlineKeyBackup
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable online key backup",
      QT_TR_NOOP("Download message encryption keys from and upload to the encrypted online key backup."),
      SM::TabEncryption, encryptionBackupOnlineEnabled, setEncryptionBackupOnlineEnabled, nullptr, EncryptionBackupOnlineEnabled, SettingKey::EncryptionBackupOnlineEnabled),
    // SessionKeys
    { QT_TR_NOOP("Session keys"), nullptr, SM::SessionKeyImportExport, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EncryptionCrossSigningSection
    { QT_TR_NOOP("Cross-signing"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OnlineBackupKey
    { QT_TR_NOOP("Online backup key"),
      QT_TR_NOOP("The key to decrypt online key backups. If it is cached, you can enable online key backup to store encryption keys securely encrypted on the server."),
      SM::KeyStatus, SM::TabEncryption,
      encryptionOnlineBackupKeyStatusValue,
      nullptr, {}, {}, {}, nullptr, nullptr, CORE_RUNTIME_ID(EncryptionOnlineBackupKeyStatus),
      },
    // SelfSigningKey
    { QT_TR_NOOP("Self signing key"),
      QT_TR_NOOP("The key to verify your own devices. If it is cached, verifying one of your devices will mark it verified for all your other devices and for users that have verified you."),
      SM::KeyStatus, SM::TabEncryption,
      encryptionSelfSigningKeyStatusValue,
      nullptr, {}, {}, {}, nullptr, nullptr, CORE_RUNTIME_ID(EncryptionSelfSigningKeyStatus),
      },
    // UserSigningKey
    { QT_TR_NOOP("User signing key"),
      QT_TR_NOOP("The key to verify other users. If it is cached, verifying a user will verify all their devices."),
      SM::KeyStatus, SM::TabEncryption,
      encryptionUserSigningKeyStatusValue,
      nullptr, {}, {}, {}, nullptr, nullptr, CORE_RUNTIME_ID(EncryptionUserSigningKeyStatus),
      },
    // MasterKey
    { QT_TR_NOOP("Master signing key"),
      QT_TR_NOOP("Your most important key. You don't need to have it cached, since not caching it makes it less likely it can be stolen and it is only needed to rotate your other signing keys."),
      SM::KeyStatus, SM::TabEncryption,
      encryptionMasterSigningKeyStatusValue,
      nullptr, {}, {}, {}, nullptr, nullptr, CORE_RUNTIME_ID(EncryptionMasterSigningKeyStatus),
      },
    // CrossSigningSecrets
    { QT_TR_NOOP("Cross-signing secrets"), nullptr, SM::XSignKeysRequestDownload, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
