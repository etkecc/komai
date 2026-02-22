    // ── Encryption Tab ──────────────────────────────────────────────────────

    // EncryptionKeySharingSection
    { QT_TR_NOOP("KEY SHARING"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OnlyShareKeysWithVerifiedUsers
    { QT_TR_NOOP("Send encrypted messages to verified users only"),
      QT_TR_NOOP("Requires a user to be verified to send encrypted messages to them. This improves safety but makes E2EE more tedious."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->onlyShareKeysWithVerifiedUsers(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setOnlyShareKeysWithVerifiedUsers(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ShareKeysWithTrustedUsers
    { QT_TR_NOOP("Share keys with verified users and devices"),
      QT_TR_NOOP("Automatically replies to key requests from other users if they are verified, even if that device shouldn't have access to those keys otherwise."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->shareKeysWithTrustedUsers(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setShareKeysWithTrustedUsers(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // EncryptionBackupSection
    { QT_TR_NOOP("BACKUP"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // UseOnlineKeyBackup
    { QT_TR_NOOP("Online key backup"),
      QT_TR_NOOP("Download message encryption keys from and upload to the encrypted online key backup."),
      SM::Toggle, SM::TabEncryption,
      []() -> QVariant { return I->useOnlineKeyBackup(); },
      [](const QVariant &v) -> bool {
          if (v.userType() != QMetaType::Bool) return false;
          I->setUseOnlineKeyBackup(v.toBool()); return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // SessionKeys
    { QT_TR_NOOP("Session keys"), nullptr, SM::SessionKeyImportExport, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EncryptionCrossSigningSection
    { QT_TR_NOOP("CROSS-SIGNING"), nullptr, SM::SectionTitle, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OnlineBackupKey
    { QT_TR_NOOP("Online backup key"),
      QT_TR_NOOP("The key to decrypt online key backups. If it is cached, you can enable online key backup to store encryption keys securely encrypted on the server."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // SelfSigningKey
    { QT_TR_NOOP("Self signing key"),
      QT_TR_NOOP("The key to verify your own devices. If it is cached, verifying one of your devices will mark it verified for all your other devices and for users that have verified you."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // UserSigningKey
    { QT_TR_NOOP("User signing key"),
      QT_TR_NOOP("The key to verify other users. If it is cached, verifying a user will verify all their devices."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // MasterKey
    { QT_TR_NOOP("Master signing key"),
      QT_TR_NOOP("Your most important key. You don't need to have it cached, since not caching it makes it less likely it can be stolen and it is only needed to rotate your other signing keys."),
      SM::KeyStatus, SM::TabEncryption,
      []() -> QVariant { return cache::secret(mtx::secret_storage::secrets::cross_signing_master).has_value(); },
      nullptr, {}, {}, {}, nullptr, nullptr },
    // CrossSigningSecrets
    { QT_TR_NOOP("Cross-signing secrets"), nullptr, SM::XSignKeysRequestDownload, SM::TabEncryption,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },

