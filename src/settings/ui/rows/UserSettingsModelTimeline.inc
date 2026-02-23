    // ── Timeline Tab ────────────────────────────────────────────────────────

    // TimelineMessagesSection
    { QT_TR_NOOP("Messages"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelineBubblesEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Enable message bubbles",
      QT_TR_NOOP("Render messages in bubble-style layout."),
      SM::TabTimeline, timelineBubblesEnabled, setTimelineBubblesEnabled, nullptr, TimelineMessagesLayoutBubbles, SettingKey::TimelineMessagesLayoutBubbles),
    // TimelineSmallAvatarsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING("Use small avatars",
                           QT_TR_NOOP("Avatars are resized to fit above the message."),
                           SM::TabTimeline, timelineSmallAvatarsEnabled, setTimelineSmallAvatarsEnabled, nullptr, TimelineMessagesLayoutSmallAvatars, SettingKey::TimelineMessagesLayoutSmallAvatars),
    // TimelineShowOwnAvatarInBubbleLayout
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show your avatar next to your own messages (bubble layout)",
      QT_TR_NOOP(
        "When bubble layout is enabled, show your avatar next to your own messages. This improves left/right symmetry and makes authorship easier to scan."),
      SM::TabTimeline, timelineShowOwnAvatarInBubbleLayout, setTimelineShowOwnAvatarInBubbleLayout, nullptr, TimelineMessagesLayoutShowOwnAvatar, SettingKey::TimelineMessagesLayoutShowOwnAvatar),
    // ShowSenderUsername
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Show sender username above messages",
      QT_TR_NOOP("Control when sender usernames are displayed above messages. In bubble mode, your own username is always hidden. In smaller rooms, avatars and bubble colors are often enough context."),
      SM::TabTimeline, showSenderUsername, setShowSenderUsername, UserSettings::ShowSenderUsername,
      timelineSenderUsernameValues,
      nullptr,
      TimelineMessagesSenderUsername,
      SettingKey::TimelineMessagesSenderUsername),
    // MaxTimelineWidth
    SIMPLE_OPTIONS_INT_CONFIG_ID_SETTING("Limit timeline width",
                              QT_TR_NOOP(
                                "Set the maximum timeline message width (in pixels). This can improve readability on wide screens."),
                              SM::TabTimeline, maxTimelineWidth, setMaxTimelineWidth, 0, 20000, 20,
                              nullptr, nullptr, TimelineMessagesMaxWidthPx, SettingKey::TimelineMessagesMaxWidthPx),
    // EnlargeEmojiOnlyMessages
    SIMPLE_BOOL_CONFIG_ID_SETTING("Enlarge emoji-only messages",
                        QT_TR_NOOP("Make font size larger if messages with only a few emojis are displayed."),
                        SM::TabTimeline, enlargeEmojiOnlyMessages, setEnlargeEmojiOnlyMessages, nullptr, TimelineMessagesEmojiOnlyEnlarge, SettingKey::TimelineMessagesEmojiOnlyEnlarge),
    // MessageHoverHighlight
    SIMPLE_BOOL_CONFIG_ID_SETTING("Highlight message on hover",
                        QT_TR_NOOP("Change the background color of messages when you hover over them."),
                        SM::TabTimeline, messageHoverHighlight, setMessageHoverHighlight, nullptr, TimelineMessagesHoverHighlight, SettingKey::TimelineMessagesHoverHighlight),
    // TimelineMessageActionsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show action buttons",
      QT_TR_NOOP("Show quick actions next to each message (react, reply, forward, and more)."),
      SM::TabTimeline, timelineMessageActionsEnabled, setTimelineMessageActionsEnabled, nullptr, TimelineMessageActionsEnabled, SettingKey::TimelineMessageActionsEnabled),
    // PinnedReactions
    SIMPLE_TEXT_CONFIG_ID_SETTING(
      "Pinned reactions",
      QT_TR_NOOP("Comma-separated list of reactions always shown in the timeline hover bar (max 10). Your recent reactions fill the remaining slots up to 10 total."),
      SM::TabTimeline,
      pinnedReactions,
      setPinnedReactions,
      nullptr,
      TimelineMessageActionsPinnedReactions,
      SettingKey::TimelineMessageActionsPinnedReactions),
    // TimelineMediaSection
    { QT_TR_NOOP("Media"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelineMediaEffectsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show message effects",
      QT_TR_NOOP("Some messages can be sent with fancy effects. For example, messages sent with '/confetti' will show confetti on screen."),
      SM::TabTimeline, timelineMediaEffectsEnabled, setTimelineMediaEffectsEnabled, nullptr, TimelineMediaEffectsEnabled, SettingKey::TimelineMediaEffectsEnabled),
    // AnimateImagesOnHover
    SIMPLE_BOOL_CONFIG_ID_SETTING("Play animated images only on hover",
                        QT_TR_NOOP("Play animated media (such as GIF/WebP) only while hovering."),
                        SM::TabTimeline, animateImagesOnHover, setAnimateImagesOnHover, nullptr, TimelineMediaAnimateOnHover, SettingKey::TimelineMediaAnimateOnHover),
    // ShowImage
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Show images automatically",
      QT_TR_NOOP(
        "Choose when images are displayed automatically."),
      SM::TabTimeline, showImage, setShowImage, UserSettings::ShowImage,
      timelineShowImageValues,
      nullptr,
      TimelineMediaImageDisplay,
      SettingKey::TimelineMediaImageDisplay),
    // OpenImagesInExternalApp
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Open images in an external app",
      QT_TR_NOOP("Open images in an external app when clicking the image.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::TabTimeline, openImagesInExternalApp, setOpenImagesInExternalApp, nullptr, TimelineMediaOpenImagesExternal, SettingKey::TimelineMediaOpenImagesExternal),
    // OpenVideosInExternalApp
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Open videos in an external app",
      QT_TR_NOOP("Open videos in an external app when clicking the video.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::TabTimeline, openVideosInExternalApp, setOpenVideosInExternalApp, nullptr, TimelineMediaOpenVideosExternal, SettingKey::TimelineMediaOpenVideosExternal),
