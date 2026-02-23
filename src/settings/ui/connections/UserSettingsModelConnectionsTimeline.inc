    // Timeline
    CONNECT_SETTING_KEY(SettingKey::TimelineMessagesLayoutBubbles, bubblesChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::TimelineMessagesLayoutSmallAvatars, smallAvatarsChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::TimelineMessagesLayoutShowOwnAvatar, showOwnAvatarInBubbleLayoutChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::TimelineMessagesSenderUsername, showSenderUsernameChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::TimelineMessagesMaxWidthPx, maxTimelineWidthChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::TimelineMessagesEmojiOnlyEnlarge, enlargeEmojiOnlyMessagesChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::TimelineMessagesHoverHighlight, messageHoverHighlightChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::TimelineMessageActionsEnabled, showActionButtonsChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::TimelineMessageActionsPinnedReactions, pinnedReactionsChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::TimelineMediaEffectsEnabled, fancyEffectsChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::TimelineMediaAnimateOnHover, animateImagesOnHoverChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::TimelineMediaImageDisplay, showImageChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::TimelineMediaOpenImagesExternal, openImagesInExternalAppChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::TimelineMediaOpenVideosExternal, openVideosInExternalAppChanged, Value);
