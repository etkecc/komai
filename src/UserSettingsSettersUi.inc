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
    setSetting(bubbles_, s, &UserSettings::bubblesChanged);
}
void
UserSettings::setSmallAvatars(bool s)
{
    setSetting(smallAvatars_, s, &UserSettings::smallAvatarsChanged);
}
void
UserSettings::setEnableStickers(bool s)
{
    setSetting(enableStickers_, s, &UserSettings::enableStickersChanged);
}
void
UserSettings::setShowOwnAvatarInBubbleLayout(bool s)
{
    setSetting(showOwnAvatarInBubbleLayout_, s, &UserSettings::showOwnAvatarInBubbleLayoutChanged);
}
void
UserSettings::setPinnedReactions(const QString &s)
{
    setSetting(pinnedReactions_, s, &UserSettings::pinnedReactionsChanged);
}
void
UserSettings::setShowSenderUsername(ShowSenderUsername s)
{
    setSetting(showSenderUsername_, s, &UserSettings::showSenderUsernameChanged);
}
void
UserSettings::setAnimateImagesOnHover(bool s)
{
    setSetting(animateImagesOnHover_, s, &UserSettings::animateImagesOnHoverChanged);
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
    setSetting(roomSortOrder_, s, &UserSettings::roomSortOrderChanged);
}
void
UserSettings::setShowActionButtons(bool s)
{
    setSetting(showActionButtons_, s, &UserSettings::showActionButtonsChanged);
}
void
UserSettings::setMaxTimelineWidth(int s)
{
    setSetting(maxTimelineWidth_, s, &UserSettings::maxTimelineWidthChanged);
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
    setSetting(useCircularAvatars_, s, &UserSettings::useCircularAvatarsChanged);
}
void
UserSettings::setDecryptNotifications(bool s)
{
    setSetting(decryptNotifications_, s, &UserSettings::decryptNotificationsChanged);
}
void
UserSettings::setShowCommunityNotificationCounts(bool s)
{
    setSetting(
      showCommunityNotificationCounts_, s, &UserSettings::showCommunityNotificationCountsChanged);
}
void
UserSettings::setCompactRoomList(bool s)
{
    setSetting(compactRoomList_, s, &UserSettings::compactRoomListChanged);
}
void
UserSettings::setShowRoomListTime(bool s)
{
    setSetting(showRoomListTime_, s, &UserSettings::showRoomListTimeChanged);
}
void
UserSettings::setShowLastMessagePreview(LastMessagePreview s)
{
    setSetting(showLastMessagePreview_, s, &UserSettings::showLastMessagePreviewChanged);
}
void
UserSettings::setFancyEffects(bool s)
{
    setSetting(fancyEffects_, s, &UserSettings::fancyEffectsChanged);
}

void
UserSettings::setReducedMotion(bool state)
{
    if (state == reducedMotion_)
        return;
    reducedMotion_ = state;
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
    setSetting(privacyScreen_, s, &UserSettings::privacyScreenChanged);
}
void
UserSettings::setPrivacyScreenTimeoutSeconds(int s)
{
    setSetting(privacyScreenTimeoutSeconds_, s, &UserSettings::privacyScreenTimeoutSecondsChanged);
}
