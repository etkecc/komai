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
    if (!setCoreValue(settings::core::SettingId::ComposerInputSendKey,
                      static_cast<int>(s),
                      "composer.input.send_key"))
        return;
    sendMessageKey_ = s;
    emit sendMessageKeyChanged(s);
    save();
}
void
UserSettings::setAutoReplaceEmoji(AutoReplaceEmoji s)
{
    if (autoReplaceEmoji_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::ComposerInputAutoReplaceEmoji,
                      static_cast<int>(s),
                      "composer.input.auto_replace_emoji"))
        return;
    autoReplaceEmoji_ = s;
    emit autoReplaceEmojiChanged(s);
    save();
}
void
UserSettings::setTimelineMessageLayout(TimelineMessageLayout s)
{
    if (timelineMessageLayout_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesLayoutStyle,
                      static_cast<int>(s),
                      "timeline.messages.layout.style"))
        return;
    timelineMessageLayout_ = s;
    emit timelineMessageLayoutChanged(s);
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
UserSettings::setTimelineMessagesSenderUsername(ShowSenderUsername s)
{
    if (timelineMessagesSenderUsername_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesSenderUsername,
                      static_cast<int>(s),
                      "timeline.messages.sender.username"))
        return;
    timelineMessagesSenderUsername_ = s;
    emit timelineMessagesSenderUsernameChanged(s);
    save();
}
void
UserSettings::setTimelineMediaAnimateOnHover(bool s)
{
    if (timelineMediaAnimateOnHover_ == s)
        return;
    timelineMediaAnimateOnHover_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineMediaAnimateOnHover, s);
    emit timelineMediaAnimateOnHoverChanged(s);
    save();
}
void
UserSettings::setReadReceiptsEnabled(bool s)
{
    if (readReceiptsEnabled_ == s)
        return;
    readReceiptsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineReadReceiptsEnabled, s);
    emit readReceiptsEnabledChanged(s);
    save();
}
void
UserSettings::setComposerTypingSendEnabled(bool s)
{
    if (composerTypingSendEnabled_ == s)
        return;
    composerTypingSendEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerTypingSendEnabled, s);
    emit composerTypingSendEnabledChanged(s);
    save();
}
void
UserSettings::setTimelineTypingShowEnabled(bool s)
{
    if (timelineTypingShowEnabled_ == s)
        return;
    timelineTypingShowEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineTypingShowEnabled, s);
    emit timelineTypingShowEnabledChanged(s);
    save();
}
void
UserSettings::setRoomSortOrder(RoomSortOrder s)
{
    if (roomSortOrder_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::SidebarsRoomListSort,
                      static_cast<int>(s),
                      "sidebars.room_list.sort"))
        return;
    roomSortOrder_ = s;
    emit roomSortOrderChanged(s);
    save();
}
void
UserSettings::setTimelineMessageActionsPolicy(TimelineMessageActionsPolicy s)
{
    if (timelineMessageActionsPolicy_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessageActionsActivationPolicy,
                      static_cast<int>(s),
                      "timeline.messages.actions.activation_policy"))
        return;
    timelineMessageActionsPolicy_ = s;
    emit timelineMessageActionsPolicyChanged(s);
    save();
}
void
UserSettings::setMaxContentWidth(int s)
{
    const auto normalized = settings::core::definitions::normalizeUiLayoutContentMaxWidthPx(s);

    if (maxContentWidth_ == normalized)
        return;
    if (!setCoreValue(settings::core::SettingId::UiLayoutContentMaxWidthPx,
                      normalized,
                      SettingKey::UiLayoutContentMaxWidthPx))
        return;
    maxContentWidth_ = normalized;
    emit maxContentWidthChanged(normalized);
    save();
}
void
UserSettings::setMaxTimelineWidth(int s)
{
    if (maxTimelineWidth_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::TimelineMessagesMaxWidthPx,
                      s,
                      SettingKey::TimelineMessagesMaxWidthPx))
        return;
    maxTimelineWidth_ = s;
    emit maxTimelineWidthChanged(s);
    save();
}
void
UserSettings::setCommunityListWidth(int s)
{
    setSetting(communityListWidth_,
               settings::core::definitions::normalizeCommunitiesWidthPx(s),
               &UserSettings::communityListWidthChanged);
}
void
UserSettings::setRoomListWidth(int s)
{
    setSetting(roomListWidth_,
               settings::core::definitions::normalizeRoomListWidthPx(s),
               &UserSettings::roomListWidthChanged);
}
void
UserSettings::setNotificationsEnabled(bool s)
{
    if (notificationsEnabled_ == s)
        return;
    notificationsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::NotificationsEnabled, s);
    emit notificationsEnabledChanged(s);
    save();
}
void
UserSettings::setNotificationsAttentionOnIncoming(bool s)
{
    if (notificationsAttentionOnIncoming_ == s)
        return;
    notificationsAttentionOnIncoming_ = s;
    (void)coreStore_.set(settings::core::SettingId::NotificationsAttentionOnIncoming, s);
    emit notificationsAttentionOnIncomingChanged(s);
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
UserSettings::setNotificationMessageContentPolicy(NotificationMessageContentPolicy s)
{
    if (notificationMessageContentPolicy_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::NotificationsMessageContentPolicy,
                      static_cast<int>(s),
                      "notifications.message_content_policy"))
        return;
    notificationMessageContentPolicy_ = s;
    emit notificationMessageContentPolicyChanged(s);
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
    if (!setCoreValue(settings::core::SettingId::SidebarsRoomListLastMessagePreview,
                      static_cast<int>(s),
                      "sidebars.room_list.last_message_preview"))
        return;
    showLastMessagePreview_ = s;
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
        setTimelineMediaAnimateOnHover(true);
    }
}

void
UserSettings::setWindowFocusBlurEnabled(bool s)
{
    if (windowFocusBlurEnabled_ == s)
        return;
    windowFocusBlurEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::PrivacyWindowFocusBlurEnabled, s);
    emit windowFocusBlurEnabledChanged(s);
    save();
}
void
UserSettings::setWindowFocusBlurDelaySeconds(int s)
{
    if (windowFocusBlurDelaySeconds_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::PrivacyWindowFocusBlurDelaySeconds,
                      s,
                      SettingKey::PrivacyWindowFocusBlurDelaySeconds))
        return;
    windowFocusBlurDelaySeconds_ = s;
    emit windowFocusBlurDelaySecondsChanged(s);
    save();
}
