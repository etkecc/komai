void
UserSettings::setMarkdownEnabled(bool s)
{
    if (markdownEnabled_ == s)
        return;
    markdownEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputMarkdownEnabled, s);
    emit markdownEnabledChanged(s);
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
UserSettings::setTimelineBubblesEnabled(bool s)
{
    if (timelineBubblesEnabled_ == s)
        return;
    timelineBubblesEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesLayoutBubbles, s);
    emit timelineBubblesEnabledChanged(s);
    save();
}
void
UserSettings::setTimelineSmallAvatarsEnabled(bool s)
{
    if (timelineSmallAvatarsEnabled_ == s)
        return;
    timelineSmallAvatarsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesLayoutSmallAvatars, s);
    emit timelineSmallAvatarsEnabledChanged(s);
    save();
}
void
UserSettings::setStickersEnabled(bool s)
{
    if (stickersEnabled_ == s)
        return;
    stickersEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerExtrasStickersEnabled, s);
    emit stickersEnabledChanged(s);
    save();
}
void
UserSettings::setTimelineShowOwnAvatarInBubbleLayout(bool s)
{
    if (timelineShowOwnAvatarInBubbleLayout_ == s)
        return;
    timelineShowOwnAvatarInBubbleLayout_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessagesLayoutShowOwnAvatar, s);
    emit timelineShowOwnAvatarInBubbleLayoutChanged(s);
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
UserSettings::setReadReceiptsEnabled(bool s)
{
    if (readReceiptsEnabled_ == s)
        return;
    readReceiptsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerFeedbackReadReceipts, s);
    emit readReceiptsEnabledChanged(s);
    save();
}
void
UserSettings::setTypingNotificationsEnabled(bool s)
{
    if (typingNotificationsEnabled_ == s)
        return;
    typingNotificationsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerFeedbackTypingNotifications, s);
    emit typingNotificationsEnabledChanged(s);
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
UserSettings::setTimelineMessageActionsEnabled(bool s)
{
    if (timelineMessageActionsEnabled_ == s)
        return;
    timelineMessageActionsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMessageActionsEnabled, s);
    emit timelineMessageActionsEnabledChanged(s);
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
UserSettings::setDesktopNotificationsEnabled(bool s)
{
    if (desktopNotificationsEnabled_ == s)
        return;
    desktopNotificationsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::NotificationsDesktopEnabled, s);
    emit desktopNotificationsEnabledChanged(s);
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
UserSettings::setCircularAvatarsEnabled(bool s)
{
    if (circularAvatarsEnabled_ == s)
        return;
    circularAvatarsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiAvatarsCircular, s);
    emit circularAvatarsEnabledChanged(s);
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
UserSettings::setCommunityNotificationCountsVisible(bool s)
{
    if (communityNotificationCountsVisible_ == s)
        return;
    communityNotificationCountsVisible_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListShowCommunityCounts, s);
    emit communityNotificationCountsVisibleChanged(s);
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
UserSettings::setRoomListShowLastMessageTime(bool s)
{
    if (roomListShowLastMessageTime_ == s)
        return;
    roomListShowLastMessageTime_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListShowLastMessageTime, s);
    emit roomListShowLastMessageTimeChanged(s);
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
UserSettings::setUiAnimationsEnabled(bool state)
{
    if (state == uiAnimationsEnabled_)
        return;
    uiAnimationsEnabled_ = state;
    (void)coreStore_.set(settings::core::SettingId::UiMotionAnimationsEnabled, state);
    emit uiAnimationsEnabledChanged(state);
    save();

    // Also toggle other motion related settings
    if (!uiAnimationsEnabled_) {
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
