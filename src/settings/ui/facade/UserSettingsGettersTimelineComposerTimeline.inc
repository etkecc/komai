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
UserSettings::timelineMediaEffectsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaEffectsEnabled);
        value.has_value())
        return *value;
    return timelineMediaEffectsEnabled_;
}

UserSettings::TimelineMessagesLayoutStyle
UserSettings::timelineMessagesLayoutStyle() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesLayoutStyle);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessagesLayoutStyle::Minimal) &&
        *value <= static_cast<int>(UserSettings::TimelineMessagesLayoutStyle::Bubbles))
        return static_cast<UserSettings::TimelineMessagesLayoutStyle>(*value);
    return timelineMessagesLayoutStyle_;
}

bool
UserSettings::timelineMessagesLayoutSmallAvatars() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMessagesLayoutSmallAvatars);
        value.has_value())
        return *value;
    return timelineMessagesLayoutSmallAvatars_;
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

QString
UserSettings::timelineMessageActionsPinnedReactions() const
{
    if (const auto value = coreStore_.valueAs<std::string>(
          settings::core::SettingId::TimelineMessageActionsPinnedReactions);
        value.has_value())
        return QString::fromStdString(*value);
    return timelineMessageActionsPinnedReactions_;
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

bool
UserSettings::timelineMediaAnimateOnHover() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaAnimateOnHover);
        value.has_value())
        return *value;
    return timelineMediaAnimateOnHover_;
}

bool
UserSettings::timelineTypingShowEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineTypingShowEnabled);
        value.has_value())
        return *value;
    return timelineTypingShowEnabled_;
}

UserSettings::TimelineMessageActionsActivationPolicy
UserSettings::timelineMessageActionsActivationPolicy() const
{
    if (const auto value = coreStore_.valueAs<int>(
          settings::core::SettingId::TimelineMessageActionsActivationPolicy);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::TimelineMessageActionsActivationPolicy::OnHover) &&
        *value <= static_cast<int>(UserSettings::TimelineMessageActionsActivationPolicy::Never))
        return static_cast<UserSettings::TimelineMessageActionsActivationPolicy>(*value);
    return timelineMessageActionsActivationPolicy_;
}

bool
UserSettings::timelineReadReceiptsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineReadReceiptsEnabled);
        value.has_value())
        return *value;
    return timelineReadReceiptsEnabled_;
}

int
UserSettings::timelineMessagesMaxWidthPx() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::TimelineMessagesMaxWidthPx);
        value.has_value())
        return *value;
    return timelineMessagesMaxWidthPx_;
}
