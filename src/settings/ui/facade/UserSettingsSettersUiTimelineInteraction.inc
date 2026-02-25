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
UserSettings::setTimelineMediaEffectsEnabled(bool s)
{
    if (timelineMediaEffectsEnabled_ == s)
        return;
    timelineMediaEffectsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaEffectsEnabled, s);
    emit timelineMediaEffectsEnabledChanged(s);
    save();
}
