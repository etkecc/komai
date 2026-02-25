bool
UserSettings::privacyMaintenanceUpdateSpaceVias() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias);
        value.has_value())
        return *value;
    return privacyMaintenanceUpdateSpaceVias_;
}

bool
UserSettings::privacyMaintenanceExpireEvents() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::PrivacyMaintenanceExpireEvents);
        value.has_value())
        return *value;
    return privacyMaintenanceExpireEvents_;
}
