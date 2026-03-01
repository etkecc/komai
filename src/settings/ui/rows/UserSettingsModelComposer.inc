    // ── Composer Tab ────────────────────────────────────────────────────────

    // ComposerInputSection
    { QT_TR_NOOP("Input"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Markdown
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Send messages as <a href=\"https://commonmark.org/help/\">Markdown</a>",
      QT_TR_NOOP("Send messages using Markdown formatting. When disabled, messages are sent as plain text."),
      SM::TabComposer, composerInputMarkdownEnabled, setComposerInputMarkdownEnabled, nullptr, ComposerInputMarkdownEnabled, SettingKey::ComposerInputMarkdownEnabled),
    // SendMessageKey
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Send key",
      QT_TR_NOOP(
        "Select what Enter key combination sends the message. Shift+Enter adds a new line, unless it has been selected, in which case Enter adds a new line instead.\n\nIf an emoji picker or a mention picker is open, it is always handled first."),
      SM::TabComposer, composerInputSendKey, setComposerInputSendKey, UserSettings::SendMessageKey,
      composerSendMessageKeyValues,
      nullptr,
      ComposerInputSendKey,
      SettingKey::ComposerInputSendKey),
    // AutoReplaceEmoji
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Auto-replace emoticons with emoji",
      QT_TR_NOOP(
        "Automatically replace text emoticons like :) :D :P with their emoji equivalents when sending a message. Choose whether to replace everywhere or only at the end."),
      SM::TabComposer, composerInputAutoReplaceEmoji, setComposerInputAutoReplaceEmoji, UserSettings::AutoReplaceEmoji,
      composerAutoReplaceEmojiValues,
      nullptr,
      ComposerInputAutoReplaceEmoji,
      SettingKey::ComposerInputAutoReplaceEmoji),
    // ComposerTypingSendEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show others I am typing",
      nullptr,
      SM::TabComposer, composerTypingSendEnabled, setComposerTypingSendEnabled, nullptr, ComposerTypingSendEnabled, SettingKey::ComposerTypingSendEnabled),
    // ComposerExtrasSection
    { QT_TR_NOOP("Extras"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableStickers
    SIMPLE_BOOL_CONFIG_ID_SETTING("Enable stickers",
                        nullptr,
                        SM::TabComposer, composerExtrasStickersEnabled, setComposerExtrasStickersEnabled, nullptr, ComposerExtrasStickersEnabled, SettingKey::ComposerExtrasStickersEnabled),
