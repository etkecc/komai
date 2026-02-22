    // ── Timeline Tab ────────────────────────────────────────────────────────

    // TimelineMessagesSection
    { QT_TR_NOOP("MESSAGES"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Bubbles
    { QT_TR_NOOP("Enable message bubbles"),
      QT_TR_NOOP("Messages get a bubble background. This also triggers some layout changes (WIP)."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->bubbles(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setBubbles(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // SmallAvatars
    { QT_TR_NOOP("Use small avatars"),
      QT_TR_NOOP("Avatars are resized to fit above the message."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->smallAvatars(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setSmallAvatars(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // ShowOwnAvatarInBubbleLayout
    { QT_TR_NOOP("Show your avatar next to your own messages (bubble layout)"),
      QT_TR_NOOP("When bubble layout is enabled, show your avatar next to your own messages. This improves left/right symmetry and makes authorship easier to scan."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->showOwnAvatarInBubbleLayout(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setShowOwnAvatarInBubbleLayout(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // ShowSenderUsername
    { QT_TR_NOOP("Show sender username above messages"),
      QT_TR_NOOP("Control when sender usernames are displayed above messages. In bubble mode, your own username is always hidden. In smaller rooms, avatars and bubble colors are often enough context."),
      SM::Options, SM::TabTimeline,
      []() -> QVariant { return static_cast<int>(I->showSenderUsername()); },
      [](const QVariant &v) -> bool {
          UserSettings::ShowSenderUsername settingValue = UserSettings::ShowSenderUsername{};
          if (!readEnumSettingValue(v, settingValue))
              return false;
          I->setShowSenderUsername(settingValue);
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only in large rooms (> 16 members)"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // MaxTimelineWidth
    { QT_TR_NOOP("Limit timeline width"),
      QT_TR_NOOP("Set the max width of messages in the timeline (in pixels). This can help readability on wide screen when Komai is maximized"),
      SM::Integer, SM::TabTimeline,
    []() -> QVariant { return I->maxTimelineWidth(); },
    [](const QVariant &v) -> bool {
        int maxWidth = 0;
        if (!readSettingValue(v, maxWidth))
            return false;
        I->setMaxTimelineWidth(maxWidth);
        return true;
    },
      0, 20000, 20, nullptr, nullptr },
    // EnlargeEmojiOnlyMessages
    { QT_TR_NOOP("Enlarge emoji-only messages"),
      QT_TR_NOOP("Make font size larger if messages with only a few emojis are displayed."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->enlargeEmojiOnlyMessages(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setEnlargeEmojiOnlyMessages(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // MessageHoverHighlight
    { QT_TR_NOOP("Highlight message on hover"),
      QT_TR_NOOP("Change the background color of messages when you hover over them."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->messageHoverHighlight(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setMessageHoverHighlight(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // ShowActionButtons
    { QT_TR_NOOP("Show action buttons"),
      QT_TR_NOOP("Show quick actions next to each message (react, reply, forward, and more)."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->showActionButtons(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setShowActionButtons(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // PinnedReactions
    { QT_TR_NOOP("Pinned reactions"),
      QT_TR_NOOP("Comma-separated list of reactions always shown in the timeline hover bar (max 10). Your recent reactions fill the remaining slots up to 10 total."),
      SM::TextInput, SM::TabTimeline,
    []() -> QVariant { return I->pinnedReactions(); },
    [](const QVariant &v) -> bool {
        QString value;
        if (!readSettingValue(v, value))
            return false;
        I->setPinnedReactions(value);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // TimelineMediaSection
    { QT_TR_NOOP("MEDIA"), nullptr, SM::SectionTitle, SM::TabTimeline,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // FancyEffects
    { QT_TR_NOOP("Show message effects"),
      QT_TR_NOOP("Some messages can be sent with fancy effects. For example, messages sent with '/confetti' will show confetti on screen."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->fancyEffects(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setFancyEffects(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // AnimateImagesOnHover
    { QT_TR_NOOP("Play animated images only on hover"),
      QT_TR_NOOP("Plays media like GIFs or WEBPs only when explicitly hovering over them."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->animateImagesOnHover(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setAnimateImagesOnHover(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // ShowImage
    { QT_TR_NOOP("Show images automatically"),
      QT_TR_NOOP("If images should be automatically displayed. You can select between always showing images by default, only show them by default in private rooms or always require interaction to show images."),
      SM::Options, SM::TabTimeline,
      []() -> QVariant { return static_cast<int>(I->showImage()); },
      [](const QVariant &v) -> bool {
          UserSettings::ShowImage settingValue = UserSettings::ShowImage{};
          if (!readEnumSettingValue(v, settingValue))
              return false;
          I->setShowImage(settingValue);
          return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only in private rooms"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // OpenImagesInExternalApp
    { QT_TR_NOOP("Open images in an external app"),
      QT_TR_NOOP("Open images in an external app when clicking the image.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->openImagesInExternalApp(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setOpenImagesInExternalApp(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
    // OpenVideosInExternalApp
    { QT_TR_NOOP("Open videos in an external app"),
      QT_TR_NOOP("Open videos in an external app when clicking the video.\nNote that when this option is ON, opened files are left unencrypted on disk and must be manually deleted."),
      SM::Toggle, SM::TabTimeline,
    []() -> QVariant { return I->openVideosInExternalApp(); },
    [](const QVariant &v) -> bool {
        bool enabled = false;
        if (!readSettingValue(v, enabled))
            return false;
        I->setOpenVideosInExternalApp(enabled);
        return true;
    },
      {}, {}, {}, nullptr, nullptr },
