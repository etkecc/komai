    // ── Composer Tab ────────────────────────────────────────────────────────

    // ComposerInputSection
    { QT_TR_NOOP("Input"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Markdown
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Send messages as <a href=\"https://commonmark.org/help/\">Markdown</a>",
      QT_TR_NOOP("Send messages using Markdown formatting. When disabled, messages are sent as plain text."),
      SM::TabComposer, markdownEnabled, setMarkdownEnabled, nullptr, ComposerInputMarkdownEnabled, SettingKey::ComposerInputMarkdownEnabled),
    // SendMessageKey
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Send key",
      QT_TR_NOOP(
        "Select what Enter key combination sends the message. Shift+Enter adds a new line, unless it has been selected, in which case Enter adds a new line instead.\n\nIf an emoji picker or a mention picker is open, it is always handled first."),
      SM::TabComposer, sendMessageKey, setSendMessageKey, UserSettings::SendMessageKey,
      composerSendMessageKeyValues,
      nullptr,
      ComposerInputSendKey,
      SettingKey::ComposerInputSendKey),
    // AutoReplaceEmoji
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Auto-replace emoticons with emoji",
      QT_TR_NOOP(
        "Automatically replace text emoticons like :) :D :P with their emoji equivalents when sending a message. Choose whether to replace everywhere or only at the end."),
      SM::TabComposer, autoReplaceEmoji, setAutoReplaceEmoji, UserSettings::AutoReplaceEmoji,
      composerAutoReplaceEmojiValues,
      nullptr,
      ComposerInputAutoReplaceEmoji,
      SettingKey::ComposerInputAutoReplaceEmoji),
    // ComposerFeedbackSection
    { QT_TR_NOOP("Feedback"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TypingNotifications
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Typing notifications",
      QT_TR_NOOP("Show who is typing in a room and send your typing status to others."),
      SM::TabComposer, typingNotificationsEnabled, setTypingNotificationsEnabled, nullptr, ComposerFeedbackTypingNotifications, SettingKey::ComposerFeedbackTypingNotifications),
    // ReadReceipts
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Read receipts",
      QT_TR_NOOP("Show whether your messages were read. If your homeserver does not support read receipts, rooms may not be marked as read."),
      SM::TabComposer, readReceiptsEnabled, setReadReceiptsEnabled, nullptr, ComposerFeedbackReadReceipts, SettingKey::ComposerFeedbackReadReceipts),
    // ComposerExtrasSection
    { QT_TR_NOOP("Extras"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableStickers
    SIMPLE_BOOL_CONFIG_ID_SETTING("Enable stickers",
                        QT_TR_NOOP("Show the sticker button in the message composer, allowing you to send stickers from custom sticker packs."),
                        SM::TabComposer, stickersEnabled, setStickersEnabled, nullptr, ComposerExtrasStickersEnabled, SettingKey::ComposerExtrasStickersEnabled),
