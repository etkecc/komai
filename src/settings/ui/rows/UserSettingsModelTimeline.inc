    // ── Timeline Tab ────────────────────────────────────────────────────────

    // TimelinePresentationSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Presentation"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelinePresentationPreview
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Timeline preview"),
      nullptr,
      SM::TimelinePreview, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelineMessagesStyle
    withKeywords(
      SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Style"),
        nullptr,
        SM::TabTimeline, timelineMessagesStyle, setTimelineMessagesStyle, UserSettings::TimelineMessagesStyle,
        timelineMessagesStyleValues,
        nullptr,
        TimelineMessagesStyle,
        SettingKey::TimelineMessagesStyle),
      QT_TRANSLATE_NOOP("UserSettingsModel", "bubble, plain, chat style")),
    // TimelineMessagesLayoutPositioning
    withKeywords(
      SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Horizontal positioning"),
        QT_TRANSLATE_NOOP("UserSettingsModel", "Choose whether messages stay on one side or are positioned opposite each other by sender."),
        SM::TabTimeline, timelineMessagesLayoutPositioning, setTimelineMessagesLayoutPositioning, UserSettings::TimelineMessagesLayoutPositioning,
        timelineMessagesLayoutPositioningValues,
        nullptr,
        TimelineMessagesLayoutPositioning,
        SettingKey::TimelineMessagesLayoutPositioning),
      QT_TRANSLATE_NOOP("UserSettingsModel", "left aligned, right aligned, alignment")),
    // TimelineMessagesLayoutMaxWidthPercent
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Maximum message width (%)"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Percentage of available timeline width that a single message can occupy."),
      SM::IntegerWithDescription, SM::TabTimeline,
      getCoreIntValue<settings::core::SettingId::TimelineMessagesLayoutMaxWidthPercent,
                      &UserSettings::timelineMessagesLayoutMaxWidthPercent>,
      setSettingValue<&UserSettings::setTimelineMessagesLayoutMaxWidthPercent, int>,
      30, 100, 5,
      nullptr,
      nullptr,
      CORE_CONFIG_ID(TimelineMessagesLayoutMaxWidthPercent, SettingKey::TimelineMessagesLayoutMaxWidthPercent) },
    // TimelineUserColorCodingPolicy
    withKeywords(
      SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Sender/message color coding"),
        QT_TRANSLATE_NOOP("UserSettingsModel", "Choose how sender colors are assigned in the timeline and bubble backgrounds."),
        SM::TabTimeline, timelineUserColorCodingPolicy, setTimelineUserColorCodingPolicy,
        UserSettings::TimelineUserColorCodingPolicy,
        timelineUserColorCodingPolicyValues,
        nullptr,
        TimelineUserColorCodingPolicy,
        SettingKey::TimelineUserColorCodingPolicy),
      QT_TRANSLATE_NOOP("UserSettingsModel", "user color, sender color, name color")),
    // ShowSenderUsername
    withKeywords(
      SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show sender username above messages"),
        QT_TRANSLATE_NOOP("UserSettingsModel", "Control when sender usernames are displayed above messages. In bubble mode, your own username is always hidden. In smaller rooms, avatars and bubble colors are often enough context."),
        SM::TabTimeline, timelineMessagesSenderUsername, setTimelineMessagesSenderUsername, UserSettings::ShowSenderUsername,
        timelineSenderUsernameValues,
        nullptr,
        TimelineMessagesSenderUsername,
        SettingKey::TimelineMessagesSenderUsername),
      QT_TRANSLATE_NOOP("UserSettingsModel", "author, who sent, message author")),
    // TimelineAvatarSize
    withKeywords(
      SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Avatar size"),
        nullptr,
        SM::TabTimeline, timelineMessagesLayoutAvatarSize, setTimelineMessagesLayoutAvatarSize, UserSettings::AvatarSize,
        timelineAvatarSizeValues,
        nullptr,
        TimelineMessagesLayoutAvatarSize,
        SettingKey::TimelineMessagesLayoutAvatarSize),
      QT_TRANSLATE_NOOP("UserSettingsModel", "profile picture, picture size")),
    // TimelineShowOwnAvatarInBubbleLayout
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show avatar next to own message bubbles"),
      QT_TRANSLATE_NOOP("UserSettingsModel",
        "When bubble layout is enabled, show your avatar next to your own messages. This improves left/right symmetry and makes authorship easier to scan."),
      SM::TabTimeline, timelineMessagesLayoutShowOwnAvatar, setTimelineMessagesLayoutShowOwnAvatar, nullptr, TimelineMessagesLayoutShowOwnAvatar, SettingKey::TimelineMessagesLayoutShowOwnAvatar),
    // EnlargeEmojiOnlyMessages
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Enlarge emoji-only messages"),
                        nullptr,
                        SM::TabTimeline, timelineMessagesEmojiOnlyEnlarge, setTimelineMessagesEmojiOnlyEnlarge, nullptr, TimelineMessagesEmojiOnlyEnlarge, SettingKey::TimelineMessagesEmojiOnlyEnlarge),
    // MessageHoverHighlight
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Highlight message on hover"),
                        nullptr,
                        SM::TabTimeline, timelineMessagesHoverHighlight, setTimelineMessagesHoverHighlight, nullptr, TimelineMessagesHoverHighlight, SettingKey::TimelineMessagesHoverHighlight),
    // TimelineFormattedCodeSyntaxHighlighting
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Syntax highlight formatted code blocks"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Apply language-aware syntax highlighting to code blocks found in formatted (HTML) timeline messages."),
      SM::TabTimeline, timelineFormattedCodeSyntaxHighlighting, setTimelineFormattedCodeSyntaxHighlighting, nullptr, TimelineFormattedCodeSyntaxHighlighting, SettingKey::TimelineFormattedCodeSyntaxHighlighting),
    // TimelineDateDividersEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show date dividers"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Show centered date and time labels between messages to mark day boundaries and long gaps in the conversation. Per-message timestamps remain visible regardless."),
      SM::TabTimeline, timelineDateDividersEnabled, setTimelineDateDividersEnabled, nullptr, TimelineDateDividersEnabled, SettingKey::TimelineDateDividersEnabled),
    // TimelineMediaEffectsEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show message effects"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Some messages, especially ones with emojis like 🎊, ☀️, or 🌦️, can show a fancier <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/features/message-effects.md\">timeline effect</a>."),
      SM::TabTimeline, timelineMediaEffectsEnabled, setTimelineMediaEffectsEnabled, nullptr, TimelineMediaEffectsEnabled, SettingKey::TimelineMediaEffectsEnabled),
    // TimelineThreadsCollapseRepliesGlobal
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Collapse thread replies"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Hides thread replies from the main timeline, showing only thread root messages. Can be overridden per room in Room Info \u2192 Preferences.<br>\u26A0\uFE0F Per-thread unread tracking is not supported, so you may miss replies in older threads."),
      SM::TabTimeline, timelineThreadsCollapseReplies, setTimelineThreadsCollapseReplies, nullptr, TimelineThreadsCollapseRepliesGlobal, SettingKey::TimelineThreadsCollapseRepliesGlobal),
    // TimelineFeedbackSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Feedback"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelineTypingShowEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show when others are typing"),
      nullptr,
      SM::TabTimeline, timelineTypingShowEnabled, setTimelineTypingShowEnabled, nullptr, TimelineTypingShowEnabled, SettingKey::TimelineTypingShowEnabled),
    // TimelineReadReceiptsGlobal
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show others when I've read their messages"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "When disabled, a private read receipt is sent instead, so rooms still get marked as read for you."),
      SM::TabTimeline, timelineReadReceiptsEnabled, setTimelineReadReceiptsEnabled, nullptr, TimelineReadReceiptsGlobal, SettingKey::TimelineReadReceiptsGlobal),
    // TimelineActionsSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Actions"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TimelineMessageActionsActivationPolicy
    withKeywords(
      SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Actions activation policy"),
        QT_TRANSLATE_NOOP("UserSettingsModel", "Choose how message actions are opened."),
        SM::TabTimeline, timelineMessageActionsActivationPolicy, setTimelineMessageActionsActivationPolicy, UserSettings::TimelineMessageActionsActivationPolicy,
        timelineMessageActionsActivationPolicyValues,
        nullptr,
        TimelineMessageActionsActivationPolicy,
        SettingKey::TimelineMessageActionsActivationPolicy),
      QT_TRANSLATE_NOOP("UserSettingsModel", "hover, button, reactions, reply, react")),
    // PinnedReactions
    SIMPLE_TEXT_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Pinned reactions"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Comma-separated list of reactions pinned to the action bar. Remaining slots are filled with your most-used reactions per room."),
      SM::TabTimeline,
      timelineMessageActionsPinnedReactions,
      setTimelineMessageActionsPinnedReactions,
      nullptr,
      TimelineMessageActionsPinnedReactions,
      SettingKey::TimelineMessageActionsPinnedReactions),
    // TimelineImagesSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Images handling"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AnimateImagesOnHover
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Play animated images only on hover"),
                        nullptr,
                        SM::TabTimeline, timelineMediaAnimateOnHover, setTimelineMediaAnimateOnHover, nullptr, TimelineMediaAnimateOnHover, SettingKey::TimelineMediaAnimateOnHover),
    // ShowImage
    withKeywords(
      SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show images automatically"),
        nullptr,
        SM::TabTimeline, timelineMediaImageDisplay, setTimelineMediaImageDisplay, UserSettings::ShowImage,
        timelineShowImageValues,
        nullptr,
        TimelineMediaImageDisplay,
        SettingKey::TimelineMediaImageDisplay),
      QT_TRANSLATE_NOOP("UserSettingsModel", "inline images, image preview")),
    // OpenImagesInExternalApp
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Open in an external viewer"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Encrypted images are <a href=\"komai://media-cache\">cached</a> decrypted on disk (auto-purged periodically)."),
      SM::TabTimeline, timelineMediaOpenImagesExternal, setTimelineMediaOpenImagesExternal, nullptr, TimelineMediaOpenImagesExternal, SettingKey::TimelineMediaOpenImagesExternal),
    // TimelineVideosSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Video handling"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OpenVideosInExternalApp
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Open in an external player"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Streams when possible; encrypted or non-seekable videos are <a href=\"komai://media-cache\">cached</a> on disk (auto-purged periodically)."),
      SM::TabTimeline, timelineMediaOpenVideosExternal, setTimelineMediaOpenVideosExternal, nullptr, TimelineMediaOpenVideosExternal, SettingKey::TimelineMediaOpenVideosExternal),
    // AutoplayGifVideos
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Inline auto-play of GIF videos"),
      nullptr,
      SM::TabTimeline, timelineMediaAutoplayGifVideos, setTimelineMediaAutoplayGifVideos, nullptr, TimelineMediaAutoplayGifVideos, SettingKey::TimelineMediaAutoplayGifVideos),
    // TimelineAudioSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Audio handling"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OpenAudioInExternalApp
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Open in an external player"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Streams when possible; encrypted or non-seekable audio is <a href=\"komai://media-cache\">cached</a> on disk (auto-purged periodically)."),
      SM::TabTimeline, timelineMediaOpenAudioExternal, setTimelineMediaOpenAudioExternal, nullptr, TimelineMediaOpenAudioExternal, SettingKey::TimelineMediaOpenAudioExternal),
    // DefaultAudioPlaybackSpeed
    SIMPLE_DOUBLE_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Default playback speed"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Applied to inline audio players."),
      SM::TabTimeline, timelineMediaDefaultAudioPlaybackSpeed, setTimelineMediaDefaultAudioPlaybackSpeed,
      0.5, 3.0, 0.5,
      TimelineMediaDefaultAudioPlaybackSpeed,
      SettingKey::TimelineMediaDefaultAudioPlaybackSpeed),
