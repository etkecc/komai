bool
UserSettings::timelineMediaEffectsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaEffectsEnabled);
        value.has_value())
        return *value;
    return timelineMediaEffectsEnabled_;
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
