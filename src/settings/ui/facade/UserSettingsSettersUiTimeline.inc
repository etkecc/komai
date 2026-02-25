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
UserSettings::setTimelineMessageActionsPinnedReactions(QString s)
{
    if (timelineMessageActionsPinnedReactions_ == s)
        return;
    timelineMessageActionsPinnedReactions_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessageActionsPinnedReactions,
                         s.toStdString());
    emit timelineMessageActionsPinnedReactionsChanged(s);
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
UserSettings::setTimelineMediaAnimateOnHover(bool s)
{
    if (timelineMediaAnimateOnHover_ == s)
        return;
    timelineMediaAnimateOnHover_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaAnimateOnHover, s);
    emit timelineMediaAnimateOnHoverChanged(s);
    save();
}
void
UserSettings::setTimelineReadReceiptsEnabled(bool s)
{
    if (timelineReadReceiptsEnabled_ == s)
        return;
    timelineReadReceiptsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineReadReceiptsEnabled, s);
    emit timelineReadReceiptsEnabledChanged(s);
    save();
}
void
UserSettings::setTimelineTypingShowEnabled(bool s)
{
    if (timelineTypingShowEnabled_ == s)
        return;
    timelineTypingShowEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineTypingShowEnabled, s);
    emit timelineTypingShowEnabledChanged(s);
    save();
}
void
UserSettings::setTimelineMessageActionsActivationPolicy(TimelineMessageActionsActivationPolicy s)
{
    if (timelineMessageActionsActivationPolicy_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessageActionsActivationPolicy,
                      static_cast<int>(s),
                      "timeline.messages.actions.activation_policy"))
        return;
    timelineMessageActionsActivationPolicy_ = s;
    emit timelineMessageActionsActivationPolicyChanged(s);
    save();
}
void
UserSettings::setTimelineMessagesMaxWidthPx(int s)
{
    if (timelineMessagesMaxWidthPx_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesMaxWidthPx,
                      s,
                      SettingKey::TimelineMessagesMaxWidthPx))
        return;
    timelineMessagesMaxWidthPx_ = s;
    emit timelineMessagesMaxWidthPxChanged(s);
    save();
}
void
UserSettings::setTimelineMediaEffectsEnabled(bool s)
{
    if (timelineMediaEffectsEnabled_ == s)
        return;
    timelineMediaEffectsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaEffectsEnabled, s);
    emit timelineMediaEffectsEnabledChanged(s);
    save();
}
