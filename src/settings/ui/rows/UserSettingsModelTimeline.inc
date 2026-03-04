    // ── Timeline Tab ────────────────────────────────────────────────────────

    // TimelinePresentationSection
    { QT_TR_NOOP("Presentation"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelinePresentationPreview
    { QT_TR_NOOP("Timeline preview"),
      nullptr,
      SM::TimelinePreview, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelineMessagesStyle
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Style",
      nullptr,
      SM::TabTimeline, timelineMessagesStyle, setTimelineMessagesStyle, UserSettings::TimelineMessagesStyle,
      timelineMessagesStyleValues,
      nullptr,
      TimelineMessagesStyle,
      SettingKey::TimelineMessagesStyle),
    // TimelineMessagesPositioning
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Horizontal positioning",
      QT_TR_NOOP("Choose whether messages stay on one side or are positioned opposite each other by sender."),
      SM::TabTimeline, timelineMessagesPositioning, setTimelineMessagesPositioning, UserSettings::TimelineMessagesPositioning,
      timelineMessagesPositioningValues,
      nullptr,
      TimelineMessagesPositioning,
      SettingKey::TimelineMessagesPositioning),
    // TimelineUserColorCodingPolicy
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Sender/message color coding",
      QT_TR_NOOP("Choose how sender colors are assigned in the timeline and bubble backgrounds."),
      SM::TabTimeline, timelineUserColorCodingPolicy, setTimelineUserColorCodingPolicy,
      UserSettings::TimelineUserColorCodingPolicy,
      timelineUserColorCodingPolicyValues,
      nullptr,
      TimelineUserColorCodingPolicy,
      SettingKey::TimelineUserColorCodingPolicy),
    // ShowSenderUsername
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Show sender username above messages",
      QT_TR_NOOP("Control when sender usernames are displayed above messages. In bubble mode, your own username is always hidden. In smaller rooms, avatars and bubble colors are often enough context."),
      SM::TabTimeline, timelineMessagesSenderUsername, setTimelineMessagesSenderUsername, UserSettings::ShowSenderUsername,
      timelineSenderUsernameValues,
      nullptr,
      TimelineMessagesSenderUsername,
      SettingKey::TimelineMessagesSenderUsername),
    // TimelineSmallAvatarsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING("Use small avatars",
                           nullptr,
                           SM::TabTimeline, timelineMessagesLayoutSmallAvatars, setTimelineMessagesLayoutSmallAvatars, nullptr, TimelineMessagesLayoutSmallAvatars, SettingKey::TimelineMessagesLayoutSmallAvatars),
    // TimelineShowOwnAvatarInBubbleLayout
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show avatar next to own message bubbles",
      QT_TR_NOOP(
        "When bubble layout is enabled, show your avatar next to your own messages. This improves left/right symmetry and makes authorship easier to scan."),
      SM::TabTimeline, timelineMessagesLayoutShowOwnAvatar, setTimelineMessagesLayoutShowOwnAvatar, nullptr, TimelineMessagesLayoutShowOwnAvatar, SettingKey::TimelineMessagesLayoutShowOwnAvatar),
    // EnlargeEmojiOnlyMessages
    SIMPLE_BOOL_CONFIG_ID_SETTING("Enlarge emoji-only messages",
                        nullptr,
                        SM::TabTimeline, timelineMessagesEmojiOnlyEnlarge, setTimelineMessagesEmojiOnlyEnlarge, nullptr, TimelineMessagesEmojiOnlyEnlarge, SettingKey::TimelineMessagesEmojiOnlyEnlarge),
    // MessageHoverHighlight
    SIMPLE_BOOL_CONFIG_ID_SETTING("Highlight message on hover",
                        nullptr,
                        SM::TabTimeline, timelineMessagesHoverHighlight, setTimelineMessagesHoverHighlight, nullptr, TimelineMessagesHoverHighlight, SettingKey::TimelineMessagesHoverHighlight),
    // TimelineFormattedCodeSyntaxHighlighting
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Syntax highlight formatted code blocks",
      QT_TR_NOOP("Apply language-aware syntax highlighting to code blocks found in formatted (HTML) timeline messages."),
      SM::TabTimeline, timelineFormattedCodeSyntaxHighlighting, setTimelineFormattedCodeSyntaxHighlighting, nullptr, TimelineFormattedCodeSyntaxHighlighting, SettingKey::TimelineFormattedCodeSyntaxHighlighting),
    // TimelineMediaEffectsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show message effects",
      QT_TR_NOOP("Some messages can be sent with fancy effects. For example, messages sent with '/confetti' will show confetti on screen."),
      SM::TabTimeline, timelineMediaEffectsEnabled, setTimelineMediaEffectsEnabled, nullptr, TimelineMediaEffectsEnabled, SettingKey::TimelineMediaEffectsEnabled),
    // TimelineFeedbackSection
    { QT_TR_NOOP("Feedback"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelineTypingShowEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show when others are typing",
      nullptr,
      SM::TabTimeline, timelineTypingShowEnabled, setTimelineTypingShowEnabled, nullptr, TimelineTypingShowEnabled, SettingKey::TimelineTypingShowEnabled),
    // TimelineReadReceiptsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Read receipts",
      QT_TR_NOOP("Show whether your messages were read. If your homeserver does not support read receipts, rooms may not be marked as read."),
      SM::TabTimeline, timelineReadReceiptsEnabled, setTimelineReadReceiptsEnabled, nullptr, TimelineReadReceiptsEnabled, SettingKey::TimelineReadReceiptsEnabled),
    // TimelineActionsSection
    { QT_TR_NOOP("Actions"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelineMessageActionsActivationPolicy
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Actions activation policy",
      QT_TR_NOOP("Choose how message actions are opened."),
      SM::TabTimeline, timelineMessageActionsActivationPolicy, setTimelineMessageActionsActivationPolicy, UserSettings::TimelineMessageActionsActivationPolicy,
      timelineMessageActionsActivationPolicyValues,
      nullptr,
      TimelineMessageActionsActivationPolicy,
      SettingKey::TimelineMessageActionsActivationPolicy),
    // PinnedReactions
    SIMPLE_TEXT_CONFIG_ID_SETTING(
      "Pinned reactions",
      QT_TR_NOOP("Comma-separated list of reactions always shown in the timeline hover bar (max 10). Your recent reactions fill the remaining slots up to 10 total."),
      SM::TabTimeline,
      timelineMessageActionsPinnedReactions,
      setTimelineMessageActionsPinnedReactions,
      nullptr,
      TimelineMessageActionsPinnedReactions,
      SettingKey::TimelineMessageActionsPinnedReactions),
    // TimelineMediaSection
    { QT_TR_NOOP("Media handling"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AnimateImagesOnHover
    SIMPLE_BOOL_CONFIG_ID_SETTING("Play animated images only on hover",
                        nullptr,
                        SM::TabTimeline, timelineMediaAnimateOnHover, setTimelineMediaAnimateOnHover, nullptr, TimelineMediaAnimateOnHover, SettingKey::TimelineMediaAnimateOnHover),
    // ShowImage
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Show images automatically",
      nullptr,
      SM::TabTimeline, timelineMediaImageDisplay, setTimelineMediaImageDisplay, UserSettings::ShowImage,
      timelineShowImageValues,
      nullptr,
      TimelineMediaImageDisplay,
      SettingKey::TimelineMediaImageDisplay),
    // OpenImagesInExternalApp
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Open images in an external app",
      QT_TR_NOOP("Open images in an external app when clicking the image.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::TabTimeline, timelineMediaOpenImagesExternal, setTimelineMediaOpenImagesExternal, nullptr, TimelineMediaOpenImagesExternal, SettingKey::TimelineMediaOpenImagesExternal),
    // OpenVideosInExternalApp
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Open videos in an external app",
      QT_TR_NOOP("Open videos in an external app when clicking the video.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::TabTimeline, timelineMediaOpenVideosExternal, setTimelineMediaOpenVideosExternal, nullptr, TimelineMediaOpenVideosExternal, SettingKey::TimelineMediaOpenVideosExternal),
