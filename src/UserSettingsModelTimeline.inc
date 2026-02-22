    // ── Timeline Tab ────────────────────────────────────────────────────────

    // TimelineMessagesSection
    { nullptr, nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Bubbles
    SIMPLE_BOOL_SETTING(
      "Enable message bubbles",
      QT_TR_NOOP("Messages get a bubble background. This also triggers some layout changes (WIP)."),
      SM::TabTimeline, bubbles, setBubbles, nullptr),
    // SmallAvatars
    SIMPLE_BOOL_SETTING("Use small avatars",
                        QT_TR_NOOP("Avatars are resized to fit above the message."),
                        SM::TabTimeline, smallAvatars, setSmallAvatars, nullptr),
    // ShowOwnAvatarInBubbleLayout
    SIMPLE_BOOL_SETTING(
      "Show your avatar next to your own messages (bubble layout)",
      QT_TR_NOOP(
        "When bubble layout is enabled, show your avatar next to your own messages. This improves left/right symmetry and makes authorship easier to scan."),
      SM::TabTimeline, showOwnAvatarInBubbleLayout, setShowOwnAvatarInBubbleLayout, nullptr),
    // ShowSenderUsername
    SIMPLE_OPTIONS_ENUM_SETTING(
      "Show sender username above messages",
      QT_TR_NOOP("Control when sender usernames are displayed above messages. In bubble mode, your own username is always hidden. In smaller rooms, avatars and bubble colors are often enough context."),
      SM::TabTimeline, showSenderUsername, setShowSenderUsername, UserSettings::ShowSenderUsername,
      []() -> QVariant {
          QStringList values;
          values << QCoreApplication::translate("UserSettingsModel", "Always")
                 << QCoreApplication::translate("UserSettingsModel", "Only in large rooms (> 16 members)")
                 << QCoreApplication::translate("UserSettingsModel", "Never");
          return values;
      },
      nullptr),
    // MaxTimelineWidth
    SIMPLE_OPTIONS_INT_SETTING("Limit timeline width",
                              QT_TR_NOOP(
                                "Set the max width of messages in the timeline (in pixels). This can help readability on wide screen when Komai is maximized"),
                              SM::TabTimeline, maxTimelineWidth, setMaxTimelineWidth, 0, 20000, 20,
                              nullptr, nullptr),
    // EnlargeEmojiOnlyMessages
    SIMPLE_BOOL_SETTING("Enlarge emoji-only messages",
                        QT_TR_NOOP("Make font size larger if messages with only a few emojis are displayed."),
                        SM::TabTimeline, enlargeEmojiOnlyMessages, setEnlargeEmojiOnlyMessages, nullptr),
    // MessageHoverHighlight
    SIMPLE_BOOL_SETTING("Highlight message on hover",
                        QT_TR_NOOP("Change the background color of messages when you hover over them."),
                        SM::TabTimeline, messageHoverHighlight, setMessageHoverHighlight, nullptr),
    // ShowActionButtons
    SIMPLE_BOOL_SETTING(
      "Show action buttons",
      QT_TR_NOOP("Show quick actions next to each message (react, reply, forward, and more)."),
      SM::TabTimeline, showActionButtons, setShowActionButtons, nullptr),
    // PinnedReactions
    SIMPLE_TEXT_SETTING(
      "Pinned reactions",
      QT_TR_NOOP("Comma-separated list of reactions always shown in the timeline hover bar (max 10). Your recent reactions fill the remaining slots up to 10 total."),
      SM::TabTimeline,
      pinnedReactions,
      setPinnedReactions,
      nullptr),
    // TimelineMediaSection
    { nullptr, nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // FancyEffects
    SIMPLE_BOOL_SETTING(
      "Show message effects",
      QT_TR_NOOP("Some messages can be sent with fancy effects. For example, messages sent with '/confetti' will show confetti on screen."),
      SM::TabTimeline, fancyEffects, setFancyEffects, nullptr),
    // AnimateImagesOnHover
    SIMPLE_BOOL_SETTING("Play animated images only on hover",
                        QT_TR_NOOP("Plays media like GIFs or WEBPs only when explicitly hovering over them."),
                        SM::TabTimeline, animateImagesOnHover, setAnimateImagesOnHover, nullptr),
    // ShowImage
    SIMPLE_OPTIONS_ENUM_SETTING(
      "Show images automatically",
      QT_TR_NOOP(
        "If images should be automatically displayed. You can select between always showing images by default, only show them by default in private rooms or always require interaction to show images."),
      SM::TabTimeline, showImage, setShowImage, UserSettings::ShowImage,
      []() -> QVariant {
          QStringList values;
          values << QCoreApplication::translate("UserSettingsModel", "Always")
                 << QCoreApplication::translate("UserSettingsModel", "Only in private rooms")
                 << QCoreApplication::translate("UserSettingsModel", "Never");
          return values;
      },
      nullptr),
    // OpenImagesInExternalApp
    SIMPLE_BOOL_SETTING(
      "Open images in an external app",
      QT_TR_NOOP("Open images in an external app when clicking the image.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::TabTimeline, openImagesInExternalApp, setOpenImagesInExternalApp, nullptr),
    // OpenVideosInExternalApp
    SIMPLE_BOOL_SETTING(
      "Open videos in an external app",
      QT_TR_NOOP("Open videos in an external app when clicking the video.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::TabTimeline, openVideosInExternalApp, setOpenVideosInExternalApp, nullptr),
