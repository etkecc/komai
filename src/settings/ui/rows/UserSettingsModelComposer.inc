    // ── Composer Tab ────────────────────────────────────────────────────────

    // ComposerInputSection
    { QT_TR_NOOP("Input"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // SendMessageKey
    SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(
      "Send key",
      QT_TR_NOOP(
        "Choose which Enter key combination sends messages."),
      SM::TabComposer, composerInputSendKey, setComposerInputSendKey, UserSettings::SendMessageKey,
      composerSendMessageKeyValues,
      nullptr,
      ComposerInputSendKey,
      SettingKey::ComposerInputSendKey),
    // MarkdownToHtml
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Auto-convert Markdown to HTML",
      QT_TR_NOOP("Controls whether <a href=\"https://commonmark.org/help/\">Markdown</a> syntax is auto-converted to produce a <a href=\"https://spec.matrix.org/v1.17/client-server-api/#mroommessage-msgtypes\">Matrix HTML</a> message with Markdown plain-text fallback."),
      SM::TabComposer, composerInputMarkdownToHtmlEnabled, setComposerInputMarkdownToHtmlEnabled, nullptr, ComposerInputMarkdownToHtmlEnabled, SettingKey::ComposerInputMarkdownToHtmlEnabled),
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
    // ComposerFeedbackSection
    { QT_TR_NOOP("Feedback"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ComposerTypingSendEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(
      "Show others when I'm typing",
      nullptr,
      SM::TabComposer, composerTypingSendEnabled, setComposerTypingSendEnabled, nullptr, ComposerTypingSendEnabled, SettingKey::ComposerTypingSendEnabled),
    // ComposerExtrasSection
    { QT_TR_NOOP("Extras"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableStickers
    SIMPLE_BOOL_CONFIG_ID_SETTING("Enable stickers",
                        nullptr,
                        SM::TabComposer, composerExtrasStickersEnabled, setComposerExtrasStickersEnabled, nullptr, ComposerExtrasStickersEnabled, SettingKey::ComposerExtrasStickersEnabled),
