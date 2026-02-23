    // Encryption
    CONNECT_SETTING_KEY(
      SettingKey::EncryptionKeySharingOnlyVerifiedUsers, onlyShareKeysWithVerifiedUsersChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::EncryptionKeySharingShareWithTrusted, shareKeysWithTrustedUsersChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::EncryptionBackupOnlineEnabled, useOnlineKeyBackupChanged, Value);

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
