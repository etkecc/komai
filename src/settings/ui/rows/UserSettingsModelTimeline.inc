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
    // TimelineMessagesLayoutPositioning
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Horizontal positioning",
      QT_TR_NOOP("Choose whether messages stay on one side or are positioned opposite each other by sender."),
      SM::TabTimeline, timelineMessagesLayoutPositioning, setTimelineMessagesLayoutPositioning, UserSettings::TimelineMessagesLayoutPositioning,
      timelineMessagesLayoutPositioningValues,
      nullptr,
      TimelineMessagesLayoutPositioning,
      SettingKey::TimelineMessagesLayoutPositioning),
    // TimelineMessagesLayoutMaxWidthPercent
    { QT_TR_NOOP("Maximum message width (%)"),
      QT_TR_NOOP("Percentage of available timeline width that a single message can occupy."),
      SM::IntegerWithDescription, SM::TabTimeline,
      getCoreIntValue<settings::core::SettingId::TimelineMessagesLayoutMaxWidthPercent,
                      &UserSettings::timelineMessagesLayoutMaxWidthPercent>,
      setSettingValue<&UserSettings::setTimelineMessagesLayoutMaxWidthPercent, int>,
      30, 100, 5,
      nullptr,
      nullptr,
      CORE_CONFIG_ID(TimelineMessagesLayoutMaxWidthPercent, SettingKey::TimelineMessagesLayoutMaxWidthPercent) },
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
    // TimelineAvatarSize
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Avatar size",
      nullptr,
      SM::TabTimeline, timelineMessagesLayoutAvatarSize, setTimelineMessagesLayoutAvatarSize, UserSettings::AvatarSize,
      timelineAvatarSizeValues,
      nullptr,
      TimelineMessagesLayoutAvatarSize,
      SettingKey::TimelineMessagesLayoutAvatarSize),
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
      QT_TR_NOOP("Some messages, especially ones with emojis like 🎊, ☀️, or 🌦️, can show a fancier <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/message-effects.md\">timeline effect</a>."),
      SM::TabTimeline, timelineMediaEffectsEnabled, setTimelineMediaEffectsEnabled, nullptr, TimelineMediaEffectsEnabled, SettingKey::TimelineMediaEffectsEnabled),
    // TimelineThreadsCollapseRepliesGlobal
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Collapse thread replies",
      QT_TR_NOOP("Hides thread replies from the main timeline, showing only thread root messages. Can be overridden per room in Room Info \u2192 Settings.<br>\u26A0\uFE0F Per-thread unread tracking is not supported, so you may miss replies in older threads."),
      SM::TabTimeline, timelineThreadsCollapseReplies, setTimelineThreadsCollapseReplies, nullptr, TimelineThreadsCollapseRepliesGlobal, SettingKey::TimelineThreadsCollapseRepliesGlobal),
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
      QT_TR_NOOP("Comma-separated list of reactions pinned to the action bar. Remaining slots are filled with your most-used reactions per room."),
      SM::TabTimeline,
      timelineMessageActionsPinnedReactions,
      setTimelineMessageActionsPinnedReactions,
      nullptr,
      TimelineMessageActionsPinnedReactions,
      SettingKey::TimelineMessageActionsPinnedReactions),
    // TimelineImagesSection
    { QT_TR_NOOP("Images handling"), nullptr, SM::SectionTitle, SM::TabTimeline,
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
      "Open in an external viewer",
      QT_TR_NOOP("Encrypted images are <a href=\"komai://media-cache\">cached</a> decrypted on disk (auto-purged periodically)."),
      SM::TabTimeline, timelineMediaOpenImagesExternal, setTimelineMediaOpenImagesExternal, nullptr, TimelineMediaOpenImagesExternal, SettingKey::TimelineMediaOpenImagesExternal),
    // TimelineVideosSection
    { QT_TR_NOOP("Video handling"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OpenVideosInExternalApp
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Open in an external player",
      QT_TR_NOOP("Streams when possible; encrypted or non-seekable videos are <a href=\"komai://media-cache\">cached</a> on disk (auto-purged periodically)."),
      SM::TabTimeline, timelineMediaOpenVideosExternal, setTimelineMediaOpenVideosExternal, nullptr, TimelineMediaOpenVideosExternal, SettingKey::TimelineMediaOpenVideosExternal),
    // AutoplayGifVideos
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Inline auto-play of GIF videos",
      nullptr,
      SM::TabTimeline, timelineMediaAutoplayGifVideos, setTimelineMediaAutoplayGifVideos, nullptr, TimelineMediaAutoplayGifVideos, SettingKey::TimelineMediaAutoplayGifVideos),
    // TimelineAudioSection
    { QT_TR_NOOP("Audio handling"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // OpenAudioInExternalApp
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Open in an external player",
      QT_TR_NOOP("Streams when possible; encrypted or non-seekable audio is <a href=\"komai://media-cache\">cached</a> on disk (auto-purged periodically)."),
      SM::TabTimeline, timelineMediaOpenAudioExternal, setTimelineMediaOpenAudioExternal, nullptr, TimelineMediaOpenAudioExternal, SettingKey::TimelineMediaOpenAudioExternal),
    // DefaultAudioPlaybackSpeed
    SIMPLE_DOUBLE_CONFIG_ID_SETTING(
      "Default playback speed",
      QT_TR_NOOP("Applied to inline audio players."),
      SM::TabTimeline, timelineMediaDefaultAudioPlaybackSpeed, setTimelineMediaDefaultAudioPlaybackSpeed,
      0.5, 3.0, 0.5,
      TimelineMediaDefaultAudioPlaybackSpeed,
      SettingKey::TimelineMediaDefaultAudioPlaybackSpeed),
