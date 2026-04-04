void
UserSettings::setTimelineMaintenanceExpireEvents(bool s)
{
    if (timelineMaintenanceExpireEvents_ == s)
        return;
    timelineMaintenanceExpireEvents_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMaintenanceExpireEvents, s);
    emit timelineMaintenanceExpireEventsChanged(s);
    save();
}
