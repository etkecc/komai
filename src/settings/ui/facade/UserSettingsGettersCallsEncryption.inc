bool
UserSettings::encryptionKeySharingShareWithTrusted() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::EncryptionKeySharingShareWithTrusted);
        value.has_value())
        return *value;
    return encryptionKeySharingShareWithTrusted_;
}
bool
UserSettings::encryptionKeySharingOnlyVerifiedUsers() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::EncryptionKeySharingOnlyVerifiedUsers);
        value.has_value())
        return *value;
    return encryptionKeySharingOnlyVerifiedUsers_;
}
bool
UserSettings::encryptionBackupOnlineEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::EncryptionBackupOnlineEnabled);
        value.has_value())
        return *value;
    return encryptionBackupOnlineEnabled_;
}
