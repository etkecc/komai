void
UserSettings::setTimelineMessagesLayoutStyle(TimelineMessagesLayoutStyle s)
{
    if (timelineMessagesLayoutStyle_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesLayoutStyle,
                      static_cast<int>(s),
                      "timeline.messages.layout.style"))
        return;
    timelineMessagesLayoutStyle_ = s;
    emit timelineMessagesLayoutStyleChanged(s);
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
