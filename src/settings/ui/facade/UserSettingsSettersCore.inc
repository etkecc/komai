template<typename T, typename Signal>
void
UserSettings::setSetting(T &member, const T &value, Signal signal)
{
    if (member == value)
        return;
    member = value;
    emit(this->*signal)(value);
    save();
}

void
UserSettings::setMessageHoverHighlight(bool s)
{
    if (messageHoverHighlight_ == s)
        return;
    messageHoverHighlight_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesHoverHighlight, s);
    emit messageHoverHighlightChanged(s);
    save();
}
void
UserSettings::setEnlargeEmojiOnlyMessages(bool s)
{
    if (enlargeEmojiOnlyMessages_ == s)
        return;
    enlargeEmojiOnlyMessages_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesEmojiOnlyEnlarge, s);
    emit enlargeEmojiOnlyMessagesChanged(s);
    save();
}
void
UserSettings::setTray(bool s)
{
    if (tray_ == s)
        return;
    tray_ = s;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsSystemTrayEnabled, s);
    emit trayChanged(s);
    save();
}
void
UserSettings::setStartInTray(bool s)
{
    if (startInTray_ == s)
        return;
    startInTray_ = s;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsSystemTrayAutostart, s);
    emit startInTrayChanged(s);
    save();
}
void
UserSettings::setTextSelectionEnabled(bool s)
{
    if (textSelectionEnabled_ == s)
        return;
    textSelectionEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiInputEnableTextSelection, s);
    emit textSelectionEnabledChanged(s);
    save();
}
void
UserSettings::setEnableSwipeGestures(bool s)
{
    if (enableSwipeGestures_ == s)
        return;
    enableSwipeGestures_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiInputSwipeGestures, s);
    emit enableSwipeGesturesChanged(s);
    save();
}
void
UserSettings::setShowCommunitiesSidebar(bool s)
{
    if (showCommunitiesSidebar_ == s)
        return;
    showCommunitiesSidebar_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsCommunitiesVisible, s);
    emit showCommunitiesSidebarChanged(s);
    save();
}
void
UserSettings::setScrollbarsInRoomlist(bool s)
{
    if (scrollbarsInRoomlist_ == s)
        return;
    scrollbarsInRoomlist_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListScrollbarsEnabled, s);
    emit scrollbarsInRoomlistChanged(s);
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
    if (access < IntegrationsDbusAccessNone || access > IntegrationsDbusAccessReadWrite)
        return;
    if (integrationsDbusApiAccess_ == access)
        return;
    integrationsDbusApiAccess_ = access;
    (void)coreStore_.set(settings::core::SettingId::IntegrationsDbusApiAccess, access);
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
UserSettings::setMaxDbs(uint s)
{
    setSetting(maxDbs_, s, &UserSettings::maxDbsChanged);
}
void
UserSettings::setRunWithoutSecureSecretsService(bool s)
{
    setSetting(
      runWithoutSecureSecretsService_, s, &UserSettings::runWithoutSecureSecretsServiceChanged);
}
void
UserSettings::setEnableHttp3(bool s)
{
    setSetting(enableHttp3_, s, &UserSettings::enableHttp3Changed);
}
