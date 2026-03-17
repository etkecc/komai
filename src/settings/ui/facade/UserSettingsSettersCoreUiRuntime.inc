void
UserSettings::setTimelineMessagesHoverHighlight(bool s)
{
    if (timelineMessagesHoverHighlight_ == s)
        return;
    timelineMessagesHoverHighlight_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesHoverHighlight, s);
    emit timelineMessagesHoverHighlightChanged(s);
    save();
}
void
UserSettings::setTimelineMessagesEmojiOnlyEnlarge(bool s)
{
    if (timelineMessagesEmojiOnlyEnlarge_ == s)
        return;
    timelineMessagesEmojiOnlyEnlarge_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge, s);
    emit timelineMessagesEmojiOnlyEnlargeChanged(s);
    save();
}
void
UserSettings::setTimelineFormattedCodeSyntaxHighlighting(bool s)
{
    if (timelineFormattedCodeSyntaxHighlighting_ == s)
        return;
    timelineFormattedCodeSyntaxHighlighting_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineFormattedCodeSyntaxHighlighting, s);
    emit timelineFormattedCodeSyntaxHighlightingChanged(s);
    save();
}
void
UserSettings::setUiInputMode(bool s)
{
    if (uiInputMode_ == s)
        return;
    uiInputMode_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiInputMode, s);
    emit uiInputModeChanged(s);
    save();
}
void
UserSettings::setUiInputTouchSwipeGesturesEnabled(bool s)
{
    if (uiInputTouchSwipeGesturesEnabled_ == s)
        return;
    uiInputTouchSwipeGesturesEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiInputTouchSwipeGesturesEnabled, s);
    emit uiInputTouchSwipeGesturesEnabledChanged(s);
    save();
}
void
UserSettings::setSidebarsCommunitiesVisible(bool s)
{
    if (sidebarsCommunitiesVisible_ == s)
        return;
    sidebarsCommunitiesVisible_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsCommunitiesVisible, s);
    emit sidebarsCommunitiesVisibleChanged(s);
    save();
}
void
UserSettings::setSidebarsCommunitiesFilterFavourites(bool s)
{
    if (sidebarsCommunitiesFilterFavourites_ == s)
        return;
    sidebarsCommunitiesFilterFavourites_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsCommunitiesFilterFavourites, s);
    emit sidebarsCommunitiesFilterFavouritesChanged(s);
    save();
}
void
UserSettings::setSidebarsCommunitiesFilterPeople(bool s)
{
    if (sidebarsCommunitiesFilterPeople_ == s)
        return;
    sidebarsCommunitiesFilterPeople_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsCommunitiesFilterPeople, s);
    emit sidebarsCommunitiesFilterPeopleChanged(s);
    save();
}
void
UserSettings::setSidebarsCommunitiesFilterBots(bool s)
{
    if (sidebarsCommunitiesFilterBots_ == s)
        return;
    sidebarsCommunitiesFilterBots_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsCommunitiesFilterBots, s);
    emit sidebarsCommunitiesFilterBotsChanged(s);
    save();
}
void
UserSettings::setSidebarsCommunitiesFilterGroups(bool s)
{
    if (sidebarsCommunitiesFilterGroups_ == s)
        return;
    sidebarsCommunitiesFilterGroups_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsCommunitiesFilterGroups, s);
    emit sidebarsCommunitiesFilterGroupsChanged(s);
    save();
}
void
UserSettings::setSidebarsCommunitiesFilterServerNotices(bool s)
{
    if (sidebarsCommunitiesFilterServerNotices_ == s)
        return;
    sidebarsCommunitiesFilterServerNotices_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsCommunitiesFilterServerNotices, s);
    emit sidebarsCommunitiesFilterServerNoticesChanged(s);
    save();
}
void
UserSettings::setSidebarsCommunitiesFilterLowPriority(bool s)
{
    if (sidebarsCommunitiesFilterLowPriority_ == s)
        return;
    sidebarsCommunitiesFilterLowPriority_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsCommunitiesFilterLowPriority, s);
    emit sidebarsCommunitiesFilterLowPriorityChanged(s);
    save();
}
void
UserSettings::setUiScrollbarPolicy(ScrollbarPolicy s)
{
    if (uiScrollbarPolicy_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::UiScrollbarPolicy,
                      static_cast<int>(s),
                      "ui.scrollbar_policy"))
        return;
    uiScrollbarPolicy_ = s;
    emit uiScrollbarPolicyChanged(s);
    save();
}
