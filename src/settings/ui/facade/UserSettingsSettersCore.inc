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
UserSettings::setIntegrationsSystemTrayEnabled(bool s)
{
    if (integrationsSystemTrayEnabled_ == s)
        return;
    integrationsSystemTrayEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsSystemTrayEnabled, s);
    emit integrationsSystemTrayEnabledChanged(s);
    save();
}
void
UserSettings::setIntegrationsSystemTrayAutostart(bool s)
{
    if (integrationsSystemTrayAutostart_ == s)
        return;
    integrationsSystemTrayAutostart_ = s;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsSystemTrayAutostart, s);
    emit integrationsSystemTrayAutostartChanged(s);
    save();
}
void
UserSettings::setTouchInputModeEnabled(bool s)
{
    if (touchInputModeEnabled_ == s)
        return;
    touchInputModeEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiInputMode, s);
    emit touchInputModeEnabledChanged(s);
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
UserSettings::setCommunitiesSidebarVisible(bool s)
{
    if (communitiesSidebarVisible_ == s)
        return;
    communitiesSidebarVisible_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsCommunitiesVisible, s);
    emit communitiesSidebarVisibleChanged(s);
    save();
}
void
UserSettings::setRoomListScrollbarsVisible(bool s)
{
    if (roomListScrollbarsVisible_ == s)
        return;
    roomListScrollbarsVisible_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListScrollbarsEnabled, s);
    emit roomListScrollbarsVisibleChanged(s);
    save();
}

void
UserSettings::setHiddenTags(const QStringList &hiddenTags)
{
    hiddenTags_ = hiddenTags;
    save();
}

void
UserSettings::setMutedTags(const QStringList &mutedTags)
{
    mutedTags_ = mutedTags;
    save();
}

void
UserSettings::setHiddenPins(const QStringList &hiddenTags)
{
    hiddenPins_ = hiddenTags;
    save();
    emit hiddenPinsChanged();
}

void
UserSettings::setHiddenWidgets(const QStringList &hiddenTags)
{
    hiddenWidgets_ = hiddenTags;
    save();
    emit hiddenWidgetsChanged();
}

void
UserSettings::setRecentReactions(QStringList recent)
{
    recentReactions_ = recent;
    save();
    emit recentReactionsChanged();
}

void
UserSettings::setCollapsedSpaces(QList<QStringList> spaces)
{
    collapsedSpaces_ = spaces;
    save();
}

void
UserSettings::setIntegrationsDbusApiAccess(int access)
{
    if (integrationsDbusApiAccess_ == access)
        return;
    if (!setCoreValue(settings::core::SettingId::IntegrationsDbusApiAccess,
                      access,
                      "integrations.dbus.access"))
        return;
    integrationsDbusApiAccess_ = access;
    emit integrationsDbusApiAccessChanged(access);
    save();
}
void
UserSettings::setIntegrationsLinksBrowserCommand(QString command)
{
    command = command.trimmed();
    if (integrationsLinksBrowserCommand_ == command)
        return;
    integrationsLinksBrowserCommand_ = command;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsBrowserCommand,
                         command.toStdString());
    emit integrationsLinksBrowserCommandChanged(command);
    save();
}
void
UserSettings::setUpdateSpaceVias(bool s)
{
    if (updateSpaceVias_ == s)
        return;
    updateSpaceVias_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias, s);
    emit updateSpaceViasChanged(s);
    save();
}
void
UserSettings::setExpireEvents(bool s)
{
    if (expireEvents_ == s)
        return;
    expireEvents_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyMaintenanceExpireEvents, s);
    emit expireEventsChanged(s);
    save();
}
void
UserSettings::setWindowWidth(int s)
{
    setSetting(windowWidth_, s, &UserSettings::windowWidthChanged);
}
void
UserSettings::setWindowHeight(int s)
{
    setSetting(windowHeight_, s, &UserSettings::windowHeightChanged);
}

void
UserSettings::setMaxDbSize(qulonglong s)
{
    setSetting(maxDbSize_, s, &UserSettings::maxDbSizeChanged);
}
void
UserSettings::setMaxStores(uint s)
{
    setSetting(maxStores_, s, &UserSettings::maxStoresChanged);
}
void
UserSettings::setHttp3Enabled(bool s)
{
    if (http3Enabled_ == s)
        return;
    http3Enabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::NetworkHttp3Enabled, s);
    emit http3EnabledChanged(s);
    save();
}
