bool
UserSettings::timelineMediaEffectsEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineMediaEffectsEnabled);
        value.has_value())
        return *value;
    return timelineMediaEffectsEnabled_;
}

bool
UserSettings::timelineDateDividersEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineDateDividersEnabled);
        value.has_value())
        return *value;
    return timelineDateDividersEnabled_;
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
          coreStore_.valueAs<bool>(settings::core::SettingId::TimelineReadReceiptsGlobal);
        value.has_value())
        return *value;
    return timelineReadReceiptsEnabled_;
}

QMap<QString, bool>
UserSettings::timelineReadReceiptsEnabledByRoom() const
{
    return timelineReadReceiptsEnabledByRoom_;
}

void
UserSettings::setTimelineReadReceiptsEnabledByRoom(const QMap<QString, bool> &byRoom)
{
    timelineReadReceiptsEnabledByRoom_ = byRoom;
    emit timelineReadReceiptsEnabledByRoomChanged();
    save();
}

void
UserSettings::setTimelineReadReceiptsEnabledForRoom(const QString &roomId, bool value)
{
    timelineReadReceiptsEnabledByRoom_[roomId] = value;
    emit timelineReadReceiptsEnabledByRoomChanged();
    save();
}

void
UserSettings::removeTimelineReadReceiptsEnabledForRoom(const QString &roomId)
{
    if (timelineReadReceiptsEnabledByRoom_.remove(roomId)) {
        emit timelineReadReceiptsEnabledByRoomChanged();
        save();
    }
}

QVariant
UserSettings::timelineReadReceiptsEnabledOverrideForRoom(const QString &roomId) const
{
    auto it = timelineReadReceiptsEnabledByRoom_.find(roomId);
    if (it != timelineReadReceiptsEnabledByRoom_.end())
        return QVariant(it.value());
    return QVariant();
}

bool
UserSettings::resolvedTimelineReadReceiptsEnabled(const QString &roomId) const
{
    auto it = timelineReadReceiptsEnabledByRoom_.find(roomId);
    if (it != timelineReadReceiptsEnabledByRoom_.end())
        return it.value();
    return timelineReadReceiptsEnabled();
}
