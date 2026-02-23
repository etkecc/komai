    // Encryption
    CONNECT_SETTING_ID(
      EncryptionKeySharingOnlyVerifiedUsers, onlyShareKeysWithVerifiedUsersChanged, Value);
    CONNECT_SETTING_ID(
      EncryptionKeySharingShareWithTrusted, shareKeysWithTrustedUsersChanged, Value);
    CONNECT_SETTING_ID(EncryptionBackupOnlineEnabled, useOnlineKeyBackupChanged, Value);

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
