    // Encryption
    CONNECT_SETTING(OnlyShareKeysWithVerifiedUsers, onlyShareKeysWithVerifiedUsersChanged, Value);
    CONNECT_SETTING(ShareKeysWithTrustedUsers, shareKeysWithTrustedUsersChanged, Value);
    CONNECT_SETTING(UseOnlineKeyBackup, useOnlineKeyBackupChanged, Value);

    // Cross-signing secrets (from MainWindow, not UserSettings)
    connect(MainWindow::instance(), &MainWindow::secretsChanged, this, [this]() {
        emit dataChanged(index(OnlineBackupKey), index(MasterKey), {Value, Good});
    });
