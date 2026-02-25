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
UserSettings::setUiInputModeTouchEnabled(bool s)
{
    if (uiInputModeTouchEnabled_ == s)
        return;
    uiInputModeTouchEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiInputMode, s);
    emit uiInputModeTouchEnabledChanged(s);
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
UserSettings::setSidebarsRoomListScrollbarsEnabled(bool s)
{
    if (sidebarsRoomListScrollbarsEnabled_ == s)
        return;
    sidebarsRoomListScrollbarsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListScrollbarsEnabled, s);
    emit sidebarsRoomListScrollbarsEnabledChanged(s);
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
UserSettings::setIntegrationsBrowserCommand(QString command)
{
    command = command.trimmed();
    if (integrationsBrowserCommand_ == command)
        return;
    integrationsBrowserCommand_ = command;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsBrowserCommand,
                         command.toStdString());
    emit integrationsBrowserCommandChanged(command);
    save();
}
void
UserSettings::setPrivacyMaintenanceUpdateSpaceVias(bool s)
{
    if (privacyMaintenanceUpdateSpaceVias_ == s)
        return;
    privacyMaintenanceUpdateSpaceVias_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyMaintenanceUpdateSpaceVias, s);
    emit privacyMaintenanceUpdateSpaceViasChanged(s);
    save();
}
void
UserSettings::setPrivacyMaintenanceExpireEvents(bool s)
{
    if (privacyMaintenanceExpireEvents_ == s)
        return;
    privacyMaintenanceExpireEvents_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyMaintenanceExpireEvents, s);
    emit privacyMaintenanceExpireEventsChanged(s);
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
UserSettings::setDbMaxSizeBytes(qulonglong s)
{
    setSetting(dbMaxSizeBytes_, s, &UserSettings::dbMaxSizeBytesChanged);
}
void
UserSettings::setMaxStores(uint s)
{
    setSetting(maxStores_, s, &UserSettings::maxStoresChanged);
}
void
UserSettings::setNetworkHttp3Enabled(bool s)
{
    if (networkHttp3Enabled_ == s)
        return;
    networkHttp3Enabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::NetworkHttp3Enabled, s);
    emit networkHttp3EnabledChanged(s);
    save();
}
