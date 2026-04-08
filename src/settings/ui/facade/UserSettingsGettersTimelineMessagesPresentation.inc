bool
UserSettings::timelineMessagesHoverHighlight() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesHoverHighlight);
        value.has_value())
        return *value;
    return timelineMessagesHoverHighlight_;
}

bool
UserSettings::timelineMessagesEmojiOnlyEnlarge() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge);
        value.has_value())
        return *value;
    return timelineMessagesEmojiOnlyEnlarge_;
}

bool
UserSettings::timelineFormattedCodeSyntaxHighlighting() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::TimelineFormattedCodeSyntaxHighlighting);
        value.has_value())
        return *value;
    return timelineFormattedCodeSyntaxHighlighting_;
}

UserSettings::TimelineMessagesStyle
UserSettings::timelineMessagesStyle() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesStyle);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessagesStyle::Plain) &&
        *value <= static_cast<int>(UserSettings::TimelineMessagesStyle::Bubbles))
        return static_cast<UserSettings::TimelineMessagesStyle>(*value);
    return timelineMessagesStyle_;
}

UserSettings::TimelineMessagesPositioning
UserSettings::timelineMessagesPositioning() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesPositioning);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessagesPositioning::OpposingBySender) &&
        *value <= static_cast<int>(UserSettings::TimelineMessagesPositioning::AllRight))
        return static_cast<UserSettings::TimelineMessagesPositioning>(*value);
    return timelineMessagesPositioning_;
}

UserSettings::TimelineUserColorCodingPolicy
UserSettings::timelineUserColorCodingPolicy() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineUserColorCodingPolicy);
        value.has_value() &&
        *value >=
          static_cast<int>(UserSettings::TimelineUserColorCodingPolicy::AdaptiveByRoomSize) &&
        *value <= static_cast<int>(UserSettings::TimelineUserColorCodingPolicy::MeVsOthers))
        return static_cast<UserSettings::TimelineUserColorCodingPolicy>(*value);
    return timelineUserColorCodingPolicy_;
}

UserSettings::AvatarSize
UserSettings::timelineMessagesLayoutAvatarSize() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesLayoutAvatarSize);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::AvatarSize::Regular) &&
        *value <= static_cast<int>(UserSettings::AvatarSize::Hidden))
        return static_cast<UserSettings::AvatarSize>(*value);
    return timelineMessagesLayoutAvatarSize_;
}

bool
UserSettings::timelineMessagesLayoutShowOwnAvatar() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar);
        value.has_value())
        return *value;
    return timelineMessagesLayoutShowOwnAvatar_;
}

UserSettings::ShowSenderUsername
UserSettings::timelineMessagesSenderUsername() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesSenderUsername);
        value.has_value() && *value >= static_cast<int>(UserSettings::ShowSenderUsername::Always) &&
        *value <= static_cast<int>(UserSettings::ShowSenderUsername::Never))
        return static_cast<UserSettings::ShowSenderUsername>(*value);
    return timelineMessagesSenderUsername_;
}

int
UserSettings::timelineMessagesSenderUsernameLargeRoomThreshold() const
{
    return 16;
}
