    // Encryption
    CONNECT_SETTING_KEY(
      SettingKey::EncryptionKeySharingOnlyVerifiedUsers, onlyShareKeysWithVerifiedUsersChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::EncryptionKeySharingShareWithTrusted, shareKeysWithTrustedUsersChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::EncryptionBackupOnlineEnabled, useOnlineKeyBackupChanged, Value);

    // Cross-signing secrets (from MainWindow, not UserSettings)
    connect(MainWindow::instance(), &MainWindow::secretsChanged, this, [this]() {
        emit dataChanged(index(OnlineBackupKey), index(MasterKey), {Value, Good});
    });
