    // Timeline
    CONNECT_SETTING_ID(TimelineMessagesStyle, timelineMessagesStyleChanged, Value);
    CONNECT_SETTING_ID(
      TimelineRoomHeaderButtonLabels, timelineRoomHeaderButtonLabelsChanged, Value);
    CONNECT_SETTING_ID(TimelineMessagesLayoutPositioning, timelineMessagesLayoutPositioningChanged, Value);
    CONNECT_SETTING_ID(
      TimelineUserColorCodingPolicy, timelineUserColorCodingPolicyChanged, Value);
    CONNECT_SETTING_ID(
      TimelineMessagesLayoutAvatarSize, timelineMessagesLayoutAvatarSizeChanged, Value);
    CONNECT_SETTING_ID(
      TimelineMessagesLayoutShowOwnAvatar, timelineMessagesLayoutShowOwnAvatarChanged, Value);
    CONNECT_SETTING_ID(
      TimelineMessagesLayoutMaxWidthPercent, timelineMessagesLayoutMaxWidthPercentChanged, Value);
    CONNECT_SETTING_ID(TimelineMessagesSenderUsername, timelineMessagesSenderUsernameChanged, Value);

    // When avatar size changes, the sender username description may change (hidden-avatar warning)
    if (const int senderUsernameIdx =
          rowForSettingId(settings::core::SettingId::TimelineMessagesSenderUsername);
        senderUsernameIdx >= 0) {
        connect(settings, &UserSettings::timelineMessagesLayoutAvatarSizeChanged, this,
                [this, senderUsernameIdx]() {
                    emit dataChanged(index(senderUsernameIdx), index(senderUsernameIdx),
                                     {Description});
                });
    }
    CONNECT_SETTING_ID(TimelineMessagesEmojiOnlyEnlarge, timelineMessagesEmojiOnlyEnlargeChanged, Value);
    CONNECT_SETTING_ID(TimelineMessagesHoverHighlight, timelineMessagesHoverHighlightChanged, Value);
    CONNECT_SETTING_ID(TimelineMessagesDragSelect, timelineMessagesDragSelectChanged, Value);
    CONNECT_SETTING_ID(TimelineFormattedCodeSyntaxHighlighting,
                       timelineFormattedCodeSyntaxHighlightingChanged,
                       Value);
    CONNECT_SETTING_ID(TimelineTypingShowEnabled, timelineTypingShowEnabledChanged, Value);
    CONNECT_SETTING_ID(TimelineReadReceiptsGlobal, timelineReadReceiptsEnabledChanged, Value);
    CONNECT_SETTING_ID(
      TimelineMessageActionsActivationPolicy, timelineMessageActionsActivationPolicyChanged, Value);
    CONNECT_SETTING_ID(TimelineMessageActionsPinnedReactions, timelineMessageActionsPinnedReactionsChanged, Value);
    CONNECT_SETTING_ID(TimelineMediaEffectsEnabled, timelineMediaEffectsEnabledChanged, Value);
    CONNECT_SETTING_ID(TimelineDateDividersEnabled, timelineDateDividersEnabledChanged, Value);
    CONNECT_SETTING_ID(TimelineMediaAnimateOnHover, timelineMediaAnimateOnHoverChanged, Value);
    CONNECT_SETTING_ID(TimelineMediaImageDisplay, timelineMediaImageDisplayChanged, Value);
    CONNECT_SETTING_ID(TimelineMediaOpenImagesExternal, timelineMediaOpenImagesExternalChanged, Value);
    CONNECT_SETTING_ID(TimelineMediaOpenVideosExternal, timelineMediaOpenVideosExternalChanged, Value);
    CONNECT_SETTING_ID(TimelineMediaAutoplayGifVideos, timelineMediaAutoplayGifVideosChanged, Value);
    CONNECT_SETTING_ID(TimelineMediaOpenAudioExternal, timelineMediaOpenAudioExternalChanged, Value);
    CONNECT_SETTING_ID(TimelineMediaDefaultAudioPlaybackSpeed, timelineMediaDefaultAudioPlaybackSpeedChanged, Value);
    CONNECT_SETTING_ID(TimelineThreadsCollapseRepliesGlobal, timelineThreadsCollapseRepliesChanged, Value);
