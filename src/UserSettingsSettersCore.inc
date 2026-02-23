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
    setSetting(messageHoverHighlight_, s, &UserSettings::messageHoverHighlightChanged);
}
void
UserSettings::setEnlargeEmojiOnlyMessages(bool s)
{
    setSetting(enlargeEmojiOnlyMessages_, s, &UserSettings::enlargeEmojiOnlyMessagesChanged);
}
void
UserSettings::setTray(bool s)
{
    setSetting(tray_, s, &UserSettings::trayChanged);
}
void
UserSettings::setStartInTray(bool s)
{
    setSetting(startInTray_, s, &UserSettings::startInTrayChanged);
}
void
UserSettings::setMobileMode(bool s)
{
    if (mobileMode_ == s)
        return;
    mobileMode_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiInputEnableTextSelection, !s);
    emit mobileModeChanged(s);
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
    setSetting(showCommunitiesSidebar_, s, &UserSettings::showCommunitiesSidebarChanged);
}
void
UserSettings::setScrollbarsInRoomlist(bool s)
{
    setSetting(scrollbarsInRoomlist_, s, &UserSettings::scrollbarsInRoomlistChanged);
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
    setSetting(integrationsDbusApiAccess_, access, &UserSettings::integrationsDbusApiAccessChanged);
}
void
UserSettings::setIntegrationsLinksBrowserCommand(QString command)
{
    setSetting(integrationsLinksBrowserCommand_,
               command.trimmed(),
               &UserSettings::integrationsLinksBrowserCommandChanged);
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
