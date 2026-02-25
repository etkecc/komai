void
UserSettings::setComposerInputMarkdownEnabled(bool s)
{
    if (composerInputMarkdownEnabled_ == s)
        return;
    composerInputMarkdownEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputMarkdownEnabled, s);
    emit composerInputMarkdownEnabledChanged(s);
    save();
}
void
UserSettings::setComposerInputSendKey(SendMessageKey s)
{
    if (composerInputSendKey_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::ComposerInputSendKey,
                      static_cast<int>(s),
                      "composer.input.send_key"))
        return;
    composerInputSendKey_ = s;
    emit composerInputSendKeyChanged(s);
    save();
}
void
UserSettings::setComposerInputAutoReplaceEmoji(AutoReplaceEmoji s)
{
    if (composerInputAutoReplaceEmoji_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::ComposerInputAutoReplaceEmoji,
                      static_cast<int>(s),
                      "composer.input.auto_replace_emoji"))
        return;
    composerInputAutoReplaceEmoji_ = s;
    emit composerInputAutoReplaceEmojiChanged(s);
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
UserSettings::setComposerExtrasStickersEnabled(bool s)
{
    if (composerExtrasStickersEnabled_ == s)
        return;
    composerExtrasStickersEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerExtrasStickersEnabled, s);
    emit composerExtrasStickersEnabledChanged(s);
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
UserSettings::setTimelineReadReceiptsEnabled(bool s)
{
    if (timelineReadReceiptsEnabled_ == s)
        return;
    timelineReadReceiptsEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::TimelineReadReceiptsEnabled, s);
    emit timelineReadReceiptsEnabledChanged(s);
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
UserSettings::setSidebarsRoomListSort(RoomSortOrder s)
{
    if (sidebarsRoomListSort_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::SidebarsRoomListSort,
                      static_cast<int>(s),
                      "sidebars.room_list.sort"))
        return;
    sidebarsRoomListSort_ = s;
    emit sidebarsRoomListSortChanged(s);
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
UserSettings::setUiLayoutContentMaxWidthPx(int s)
{
    const auto normalized = settings::core::definitions::normalizeUiLayoutContentMaxWidthPx(s);

    if (uiLayoutContentMaxWidthPx_ == normalized)
        return;
    if (!setCoreValue(settings::core::SettingId::UiLayoutContentMaxWidthPx,
                      normalized,
                      SettingKey::UiLayoutContentMaxWidthPx))
        return;
    uiLayoutContentMaxWidthPx_ = normalized;
    emit uiLayoutContentMaxWidthPxChanged(normalized);
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
UserSettings::setUiAvatarsCircular(bool s)
{
    if (uiAvatarsCircular_ == s)
        return;
    uiAvatarsCircular_ = s;
    (void)coreStore_.set(settings::core::SettingId::UiAvatarsCircular, s);
    emit uiAvatarsCircularChanged(s);
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
UserSettings::setSidebarsRoomListShowCommunityCounts(bool s)
{
    if (sidebarsRoomListShowCommunityCounts_ == s)
        return;
    sidebarsRoomListShowCommunityCounts_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListShowCommunityCounts, s);
    emit sidebarsRoomListShowCommunityCountsChanged(s);
    save();
}
void
UserSettings::setSidebarsRoomListCompact(bool s)
{
    if (sidebarsRoomListCompact_ == s)
        return;
    sidebarsRoomListCompact_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListCompact, s);
    emit sidebarsRoomListCompactChanged(s);
    save();
}
void
UserSettings::setSidebarsRoomListShowLastMessageTime(bool s)
{
    if (sidebarsRoomListShowLastMessageTime_ == s)
        return;
    sidebarsRoomListShowLastMessageTime_ = s;
    (void)coreStore_.set(settings::core::SettingId::SidebarsRoomListShowLastMessageTime, s);
    emit sidebarsRoomListShowLastMessageTimeChanged(s);
    save();
}
void
UserSettings::setSidebarsRoomListLastMessagePreview(LastMessagePreview s)
{
    if (sidebarsRoomListLastMessagePreview_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::SidebarsRoomListLastMessagePreview,
                      static_cast<int>(s),
                      "sidebars.room_list.last_message_preview"))
        return;
    sidebarsRoomListLastMessagePreview_ = s;
    emit sidebarsRoomListLastMessagePreviewChanged(s);
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
UserSettings::setUiMotionAnimationsEnabled(bool state)
{
    if (state == uiMotionAnimationsEnabled_)
        return;
    uiMotionAnimationsEnabled_ = state;
    (void)coreStore_.set(settings::core::SettingId::UiMotionAnimationsEnabled, state);
    emit uiMotionAnimationsEnabledChanged(state);
    save();

    // Also toggle other motion related settings
    if (!uiMotionAnimationsEnabled_) {
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
