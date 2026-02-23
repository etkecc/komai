void
UserSettings::setMarkdown(bool s)
{
    setSetting(markdown_, s, &UserSettings::markdownChanged);
}
void
UserSettings::setSendMessageKey(SendMessageKey s)
{
    setSetting(sendMessageKey_, s, &UserSettings::sendMessageKeyChanged);
}
void
UserSettings::setAutoReplaceEmoji(AutoReplaceEmoji s)
{
    setSetting(autoReplaceEmoji_, s, &UserSettings::autoReplaceEmojiChanged);
}
void
UserSettings::setBubbles(bool s)
{
    if (bubbles_ == s)
        return;
    bubbles_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesLayoutBubbles, s);
    emit bubblesChanged(s);
    save();
}
void
UserSettings::setSmallAvatars(bool s)
{
    if (smallAvatars_ == s)
        return;
    smallAvatars_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesLayoutSmallAvatars, s);
    emit smallAvatarsChanged(s);
    save();
}
void
UserSettings::setEnableStickers(bool s)
{
    setSetting(enableStickers_, s, &UserSettings::enableStickersChanged);
}
void
UserSettings::setShowOwnAvatarInBubbleLayout(bool s)
{
    if (showOwnAvatarInBubbleLayout_ == s)
        return;
    showOwnAvatarInBubbleLayout_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar, s);
    emit showOwnAvatarInBubbleLayoutChanged(s);
    save();
}
void
UserSettings::setPinnedReactions(QString s)
{
    if (pinnedReactions_ == s)
        return;
    pinnedReactions_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessageActionsPinnedReactions,
                         s.toStdString());
    emit pinnedReactionsChanged(s);
    save();
}
void
UserSettings::setShowSenderUsername(ShowSenderUsername s)
{
    if (showSenderUsername_ == s)
        return;
    showSenderUsername_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesSenderUsername,
                         static_cast<int>(s));
    emit showSenderUsernameChanged(s);
    save();
}
void
UserSettings::setAnimateImagesOnHover(bool s)
{
    if (animateImagesOnHover_ == s)
        return;
    animateImagesOnHover_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaAnimateOnHover, s);
    emit animateImagesOnHoverChanged(s);
    save();
}
void
UserSettings::setReadReceipts(bool s)
{
    setSetting(readReceipts_, s, &UserSettings::readReceiptsChanged);
}
void
UserSettings::setTypingNotifications(bool s)
{
    setSetting(typingNotifications_, s, &UserSettings::typingNotificationsChanged);
}
void
UserSettings::setRoomSortOrder(RoomSortOrder s)
{
    if (roomSortOrder_ == s)
        return;
    roomSortOrder_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListSort, static_cast<int>(s));
    emit roomSortOrderChanged(s);
    save();
}
void
UserSettings::setShowActionButtons(bool s)
{
    if (showActionButtons_ == s)
        return;
    showActionButtons_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessageActionsEnabled, s);
    emit showActionButtonsChanged(s);
    save();
}
void
UserSettings::setMaxTimelineWidth(int s)
{
    if (maxTimelineWidth_ == s)
        return;
    maxTimelineWidth_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesMaxWidthPx, s);
    emit maxTimelineWidthChanged(s);
    save();
}
void
UserSettings::setCommunityListWidth(int s)
{
    setSetting(communityListWidth_, s, &UserSettings::communityListWidthChanged);
}
void
UserSettings::setRoomListWidth(int s)
{
    setSetting(roomListWidth_, s, &UserSettings::roomListWidthChanged);
}
void
UserSettings::setDesktopNotifications(bool s)
{
    setSetting(hasDesktopNotifications_, s, &UserSettings::desktopNotificationsChanged);
}
void
UserSettings::setAlertOnIncomingMessages(bool s)
{
    setSetting(alertOnIncomingMessages_, s, &UserSettings::alertOnIncomingMessagesChanged);
}
void
UserSettings::setUseCircularAvatars(bool s)
{
    if (useCircularAvatars_ == s)
        return;
    useCircularAvatars_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiAvatarsCircular, s);
    emit useCircularAvatarsChanged(s);
    save();
}
void
UserSettings::setDecryptNotifications(bool s)
{
    setSetting(decryptNotifications_, s, &UserSettings::decryptNotificationsChanged);
}
void
UserSettings::setShowCommunityNotificationCounts(bool s)
{
    if (showCommunityNotificationCounts_ == s)
        return;
    showCommunityNotificationCounts_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListShowCommunityCounts, s);
    emit showCommunityNotificationCountsChanged(s);
    save();
}
void
UserSettings::setCompactRoomList(bool s)
{
    if (compactRoomList_ == s)
        return;
    compactRoomList_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListCompact, s);
    emit compactRoomListChanged(s);
    save();
}
void
UserSettings::setShowRoomListTime(bool s)
{
    if (showRoomListTime_ == s)
        return;
    showRoomListTime_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListShowLastMessageTime, s);
    emit showRoomListTimeChanged(s);
    save();
}
void
UserSettings::setShowLastMessagePreview(LastMessagePreview s)
{
    if (showLastMessagePreview_ == s)
        return;
    showLastMessagePreview_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListLastMessagePreview,
                         static_cast<int>(s));
    emit showLastMessagePreviewChanged(s);
    save();
}
void
UserSettings::setFancyEffects(bool s)
{
    if (fancyEffects_ == s)
        return;
    fancyEffects_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaEffectsEnabled, s);
    emit fancyEffectsChanged(s);
    save();
}

void
UserSettings::setReducedMotion(bool state)
{
    if (state == reducedMotion_)
        return;
    reducedMotion_ = state;
    (void)coreStore_.set(settings::core::SettingId::UiMotionAnimationsEnabled, !state);
    emit reducedMotionChanged(state);
    save();

    // Also toggle other motion related settings
    if (reducedMotion_) {
        setFancyEffects(false);
        setAnimateImagesOnHover(true);
    }
}

void
UserSettings::setPrivacyScreen(bool s)
{
    if (privacyScreen_ == s)
        return;
    privacyScreen_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyScreenLockEnabled, s);
    emit privacyScreenChanged(s);
    save();
}
void
UserSettings::setPrivacyScreenTimeoutSeconds(int s)
{
    if (privacyScreenTimeoutSeconds_ == s)
        return;
    privacyScreenTimeoutSeconds_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyScreenLockTimeoutSeconds, s);
    emit privacyScreenTimeoutSecondsChanged(s);
    save();
}
