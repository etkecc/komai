void
UserSettings::setEncryptionKeySharingOnlyVerifiedUsers(bool s)
{
    if (encryptionKeySharingOnlyVerifiedUsers_ == s)
        return;
    encryptionKeySharingOnlyVerifiedUsers_ = s;
    (void)coreStore_.set(settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers, s);
    emit encryptionKeySharingOnlyVerifiedUsersChanged(s);
    save();
}
void
UserSettings::setEncryptionKeySharingShareWithTrusted(bool s)
{
    if (encryptionKeySharingShareWithTrusted_ == s)
        return;
    encryptionKeySharingShareWithTrusted_ = s;
    (void)coreStore_.set(settings::core::SettingId::EncryptionKeySharingShareWithTrusted, s);
    emit encryptionKeySharingShareWithTrustedChanged(s);
    save();
}

void
UserSettings::setEncryptionBackupOnlineEnabled(bool useBackup)
{
    if (useBackup == encryptionBackupOnlineEnabled_)
        return;

    encryptionBackupOnlineEnabled_ = useBackup;
    (void)coreStore_.set(settings::core::SettingId::EncryptionBackupOnlineEnabled, useBackup);
    emit encryptionBackupOnlineEnabledChanged(useBackup);
    save();
}

void
UserSettings::setEncryptionBackupOnlineEnabledFromConfig(bool useBackup)
{
    if (useBackup == encryptionBackupOnlineEnabled_)
        return;

    encryptionBackupOnlineEnabled_ = useBackup;
    (void)coreStore_.set(settings::core::SettingId::EncryptionBackupOnlineEnabled, useBackup);
    emit encryptionBackupOnlineEnabledChanged(useBackup);
}
