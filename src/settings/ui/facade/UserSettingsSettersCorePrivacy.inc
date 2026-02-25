void
UserSettings::setPrivacyMaintenanceUpdateSpaceVias(bool s)
{
    if (privacyMaintenanceUpdateSpaceVias_ == s)
        return;
    privacyMaintenanceUpdateSpaceVias_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias, s);
    emit privacyMaintenanceUpdateSpaceViasChanged(s);
    save();
}

void
UserSettings::setPrivacyMaintenanceExpireEvents(bool s)
{
    if (privacyMaintenanceExpireEvents_ == s)
        return;
    privacyMaintenanceExpireEvents_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyMaintenanceExpireEvents, s);
    emit privacyMaintenanceExpireEventsChanged(s);
    save();
}
