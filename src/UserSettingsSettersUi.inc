void
UserSettings::setMarkdown(bool s)
{
    if (markdown_ == s)
        return;
    markdown_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputMarkdownEnabled, s);
    emit markdownChanged(s);
    save();
}
void
UserSettings::setSendMessageKey(SendMessageKey s)
{
    if (sendMessageKey_ == s)
        return;
    sendMessageKey_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputSendKey, static_cast<int>(s));
    emit sendMessageKeyChanged(s);
    save();
}
void
UserSettings::setAutoReplaceEmoji(AutoReplaceEmoji s)
{
    if (autoReplaceEmoji_ == s)
        return;
    autoReplaceEmoji_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputAutoReplaceEmoji,
                         static_cast<int>(s));
    emit autoReplaceEmojiChanged(s);
    save();
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
    if (enableStickers_ == s)
        return;
    enableStickers_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerExtrasStickersEnabled, s);
    emit enableStickersChanged(s);
    save();
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
    if (readReceipts_ == s)
        return;
    readReceipts_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerFeedbackReadReceipts, s);
    emit readReceiptsChanged(s);
    save();
}
void
UserSettings::setTypingNotifications(bool s)
{
    if (typingNotifications_ == s)
        return;
    typingNotifications_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerFeedbackTypingNotifications, s);
    emit typingNotificationsChanged(s);
    save();
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
    if (hasDesktopNotifications_ == s)
        return;
    hasDesktopNotifications_ = s;
    (void)coreStore_.set(settings::core::SettingId::NotificationsDesktopEnabled, s);
    emit desktopNotificationsChanged(s);
    save();
}
void
UserSettings::setAlertOnIncomingMessages(bool s)
{
    if (alertOnIncomingMessages_ == s)
        return;
    alertOnIncomingMessages_ = s;
    (void)coreStore_.set(settings::core::SettingId::NotificationsDesktopAlertOnIncoming, s);
    emit alertOnIncomingMessagesChanged(s);
    save();
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
    if (decryptNotifications_ == s)
        return;
    decryptNotifications_ = s;
    (void)coreStore_.set(settings::core::SettingId::NotificationsDesktopDecryptMessages, s);
    emit decryptNotificationsChanged(s);
    save();
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
UserSettings::setTimelineMediaEffectsEnabled(bool s)
{
    if (timelineMediaEffectsEnabled_ == s)
        return;
    timelineMediaEffectsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaEffectsEnabled, s);
    emit timelineMediaEffectsEnabledChanged(s);
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
        setTimelineMediaEffectsEnabled(false);
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
