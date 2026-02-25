    // Encryption
    CONNECT_SETTING_ID(
      EncryptionKeySharingOnlyVerifiedUsers, encryptionKeySharingOnlyVerifiedUsersChanged, Value);
    CONNECT_SETTING_ID(
      EncryptionKeySharingShareWithTrusted, encryptionKeySharingShareWithTrustedChanged, Value);
    CONNECT_SETTING_ID(EncryptionBackupOnlineEnabled, encryptionBackupOnlineEnabledChanged, Value);

    // Cross-signing secrets (from MainWindow, not UserSettings)
    connect(MainWindow::instance(), &MainWindow::secretsChanged, this, [this]() {
        constexpr auto ids = std::array{
          settings::core::SettingId::EncryptionOnlineBackupKeyStatus,
          settings::core::SettingId::EncryptionSelfSigningKeyStatus,
          settings::core::SettingId::EncryptionUserSigningKeyStatus,
          settings::core::SettingId::EncryptionMasterSigningKeyStatus,
        };

        for (const auto id : ids) {
            if (const int idx = rowForSettingId(id); idx >= 0)
                emit dataChanged(index(idx), index(idx), {Value, Good});
        }
    });
