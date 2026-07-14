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
UserSettings::setTimelineMessagesDragSelect(bool s)
{
    if (timelineMessagesDragSelect_ == s)
        return;
    timelineMessagesDragSelect_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesDragSelect, s);
    emit timelineMessagesDragSelectChanged(s);
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
UserSettings::setTimelineThreadsCollapseReplies(bool s)
{
    if (timelineThreadsCollapseReplies_ == s)
        return;
    timelineThreadsCollapseReplies_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineThreadsCollapseRepliesGlobal, s);
    emit timelineThreadsCollapseRepliesChanged(s);
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
UserSettings::setNavigationCommunitiesFilterFavourites(bool s)
{
    if (navigationCommunitiesFilterFavourites_ == s)
        return;
    navigationCommunitiesFilterFavourites_ = s;
    (void)coreStore_.set(settings::core::SettingId::NavigationCommunitiesFilterFavourites, s);
    emit navigationCommunitiesFilterFavouritesChanged(s);
    save();
}
void
UserSettings::setNavigationCommunitiesFilterPeople(bool s)
{
    if (navigationCommunitiesFilterPeople_ == s)
        return;
    navigationCommunitiesFilterPeople_ = s;
    (void)coreStore_.set(settings::core::SettingId::NavigationCommunitiesFilterPeople, s);
    emit navigationCommunitiesFilterPeopleChanged(s);
    save();
}
void
UserSettings::setNavigationCommunitiesFilterBots(bool s)
{
    if (navigationCommunitiesFilterBots_ == s)
        return;
    navigationCommunitiesFilterBots_ = s;
    (void)coreStore_.set(settings::core::SettingId::NavigationCommunitiesFilterBots, s);
    emit navigationCommunitiesFilterBotsChanged(s);
    save();
}
void
UserSettings::setNavigationCommunitiesFilterGroups(bool s)
{
    if (navigationCommunitiesFilterGroups_ == s)
        return;
    navigationCommunitiesFilterGroups_ = s;
    (void)coreStore_.set(settings::core::SettingId::NavigationCommunitiesFilterGroups, s);
    emit navigationCommunitiesFilterGroupsChanged(s);
    save();
}
void
UserSettings::setNavigationCommunitiesFilterServerNotices(bool s)
{
    if (navigationCommunitiesFilterServerNotices_ == s)
        return;
    navigationCommunitiesFilterServerNotices_ = s;
    (void)coreStore_.set(settings::core::SettingId::NavigationCommunitiesFilterServerNotices, s);
    emit navigationCommunitiesFilterServerNoticesChanged(s);
    save();
}
void
UserSettings::setNavigationCommunitiesFilterLowPriority(bool s)
{
    if (navigationCommunitiesFilterLowPriority_ == s)
        return;
    navigationCommunitiesFilterLowPriority_ = s;
    (void)coreStore_.set(settings::core::SettingId::NavigationCommunitiesFilterLowPriority, s);
    emit navigationCommunitiesFilterLowPriorityChanged(s);
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
void
UserSettings::setUiThemeMode(ThemeMode s)
{
    if (uiThemeMode_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::UiThemeMode, static_cast<int>(s), "ui.theme.mode"))
        return;
    uiThemeMode_ = s;
    emit uiThemeModeChanged(s);
    save();
}
