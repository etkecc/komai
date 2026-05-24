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
UserSettings::setTimelineMessagesLayoutPositioning(TimelineMessagesLayoutPositioning s)
{
    if (timelineMessagesLayoutPositioning_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesLayoutPositioning,
                      static_cast<int>(s),
                      "timeline.messages.layout.positioning"))
        return;
    timelineMessagesLayoutPositioning_ = s;
    emit timelineMessagesLayoutPositioningChanged(s);
    save();
}

void
UserSettings::setTimelineUserColorCodingPolicy(TimelineUserColorCodingPolicy s)
{
    if (timelineUserColorCodingPolicy_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineUserColorCodingPolicy,
                      static_cast<int>(s),
                      "timeline.user_color_coding_policy"))
        return;
    timelineUserColorCodingPolicy_ = s;
    emit timelineUserColorCodingPolicyChanged(s);
    save();
}

void
UserSettings::setTimelineMessagesLayoutAvatarSize(AvatarSize s)
{
    if (timelineMessagesLayoutAvatarSize_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesLayoutAvatarSize,
                      static_cast<int>(s),
                      "timeline.messages.layout.avatar_size"))
        return;
    timelineMessagesLayoutAvatarSize_ = s;
    emit timelineMessagesLayoutAvatarSizeChanged(s);
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
UserSettings::setTimelineMessagesLayoutMaxWidthPercent(int s)
{
    if (timelineMessagesLayoutMaxWidthPercent_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesLayoutMaxWidthPercent,
                      s,
                      SettingKey::TimelineMessagesLayoutMaxWidthPercent))
        return;
    timelineMessagesLayoutMaxWidthPercent_ = s;
    emit timelineMessagesLayoutMaxWidthPercentChanged(s);
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

void
UserSettings::setTimelineRoomHeaderButtonLabels(RoomHeaderButtonLabels s)
{
    if (timelineRoomHeaderButtonLabels_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineRoomHeaderButtonLabels,
                      static_cast<int>(s),
                      SettingKey::TimelineRoomHeaderButtonLabels))
        return;
    timelineRoomHeaderButtonLabels_ = s;
    emit timelineRoomHeaderButtonLabelsChanged(s);
    save();
}
