void
UserSettings::setTimelineMessagesStyle(TimelineMessagesStyle s)
{
    if (timelineMessagesStyle_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesStyle,
                      static_cast<int>(s),
                      "timeline.messages.style"))
        return;
    timelineMessagesStyle_ = s;
    emit timelineMessagesStyleChanged(s);
    save();
}

void
UserSettings::setTimelineMessagesPositioning(TimelineMessagesPositioning s)
{
    if (timelineMessagesPositioning_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesPositioning,
                      static_cast<int>(s),
                      "timeline.messages.positioning"))
        return;
    timelineMessagesPositioning_ = s;
    emit timelineMessagesPositioningChanged(s);
    save();
}

void
UserSettings::setTimelineMessagesLayoutSmallAvatars(bool s)
{
    if (timelineMessagesLayoutSmallAvatars_ == s)
        return;
    timelineMessagesLayoutSmallAvatars_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesLayoutSmallAvatars, s);
    emit timelineMessagesLayoutSmallAvatarsChanged(s);
    save();
}
void
UserSettings::setTimelineMessagesLayoutShowOwnAvatar(bool s)
{
    if (timelineMessagesLayoutShowOwnAvatar_ == s)
        return;
    timelineMessagesLayoutShowOwnAvatar_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar, s);
    emit timelineMessagesLayoutShowOwnAvatarChanged(s);
    save();
}
void
UserSettings::setTimelineMessagesSenderUsername(ShowSenderUsername s)
{
    if (timelineMessagesSenderUsername_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesSenderUsername,
                      static_cast<int>(s),
                      "timeline.messages.sender.username"))
        return;
    timelineMessagesSenderUsername_ = s;
    emit timelineMessagesSenderUsernameChanged(s);
    save();
}
