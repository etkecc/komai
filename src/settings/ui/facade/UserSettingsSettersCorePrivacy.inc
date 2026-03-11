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
