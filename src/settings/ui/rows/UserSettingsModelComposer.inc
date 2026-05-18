    // ── Composer Tab ────────────────────────────────────────────────────────

    // ComposerInputSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Input"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // SendMessageKey
    withKeywords(
      SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Send key"),
        QT_TRANSLATE_NOOP("UserSettingsModel",
          "Choose which Enter key combination sends messages."),
        SM::TabComposer, composerInputSendKey, setComposerInputSendKey, UserSettings::SendMessageKey,
        composerSendMessageKeyValues,
        nullptr,
        ComposerInputSendKey,
        SettingKey::ComposerInputSendKey),
      QT_TRANSLATE_NOOP("UserSettingsModel", "keyboard shortcut, send key, enter, shift+enter, ctrl+enter")),
    // MarkdownToHtml
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Auto-convert Markdown to HTML"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Controls whether <a href=\"https://commonmark.org/help/\">Markdown</a> syntax is auto-converted to produce a <a href=\"https://spec.matrix.org/v1.17/client-server-api/#mroommessage-msgtypes\">Matrix HTML</a> message with Markdown plain-text fallback."),
      SM::TabComposer, composerInputMarkdownToHtmlEnabled, setComposerInputMarkdownToHtmlEnabled, nullptr, ComposerInputMarkdownToHtmlEnabled, SettingKey::ComposerInputMarkdownToHtmlEnabled),
    // InlineRoomPicker
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Inline room picker"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Show an inline room picker when you type a hash (#) in the message input."),
      SM::TabComposer, composerInputInlineRoomPickerEnabled, setComposerInputInlineRoomPickerEnabled, nullptr, ComposerInputInlineRoomPickerEnabled, SettingKey::ComposerInputInlineRoomPickerEnabled),
    // InlineUserPicker
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Inline user picker"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Show an inline user picker when you type an at sign (@) in the message input."),
      SM::TabComposer, composerInputInlineUserPickerEnabled, setComposerInputInlineUserPickerEnabled, nullptr, ComposerInputInlineUserPickerEnabled, SettingKey::ComposerInputInlineUserPickerEnabled),
    // SelectionFormattingToolbar
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Selection formatting toolbar"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Shows a toolbar with Bold, Italic, Code, Quote, and Link buttons when you select text. <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/features/keyboard-shortcuts.md#%EF%B8%8F-composer\">Keyboard shortcuts</a> work either way."),
      SM::TabComposer, composerInputSelectionFormattingToolbarEnabled, setComposerInputSelectionFormattingToolbarEnabled, nullptr, ComposerInputSelectionFormattingToolbarEnabled, SettingKey::ComposerInputSelectionFormattingToolbarEnabled),
    // ComposerInputTranscriptionEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Voice transcription"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Enables speech-to-text. Configure the transcription provider in <a href=\"komai://settings/integrations/transcription\">Integrations → Voice transcription</a>."),
      SM::TabComposer, composerInputTranscriptionEnabled, setComposerInputTranscriptionEnabled, nullptr, ComposerInputTranscriptionEnabled, SettingKey::ComposerInputTranscriptionEnabled),
    // ComposerInputSpellcheckEnabled
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Spell checking"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Underlines misspelled words in the composer and room-topic field. Pick the dictionaries to use in the Spellcheck section below."),
      SM::TabComposer, composerInputSpellcheckEnabled, setComposerInputSpellcheckEnabled, nullptr, ComposerInputSpellcheckEnabled, SettingKey::ComposerInputSpellcheckEnabled),
    // ComposerAttachmentsSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Attachments"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ComposerAttachmentsStripImageMetadata
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Strip image metadata before upload"),
      QT_TRANSLATE_NOOP("UserSettingsModel",
        "Best-effort removal of metadata (EXIF, IPTC, XMP) from some image formats to protect your privacy. <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/features/attachments.md\">Learn more</a>."),
      SM::TabComposer, composerAttachmentsStripImageMetadata, setComposerAttachmentsStripImageMetadata, nullptr, ComposerAttachmentsStripImageMetadata, SettingKey::ComposerAttachmentsStripImageMetadata),
    // ComposerEmojiSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Emoji"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // AutoReplaceEmoji
    withKeywords(
      SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Auto-replace emoticons with emoji"),
        QT_TRANSLATE_NOOP("UserSettingsModel",
          "Automatically replace text emoticons like :) :D :P with their emoji equivalents when sending a message. Choose whether to replace everywhere or only at the end."),
        SM::TabComposer, composerInputAutoReplaceEmoji, setComposerInputAutoReplaceEmoji, UserSettings::AutoReplaceEmoji,
        composerAutoReplaceEmojiValues,
        nullptr,
        ComposerInputAutoReplaceEmoji,
        SettingKey::ComposerInputAutoReplaceEmoji),
      QT_TRANSLATE_NOOP("UserSettingsModel", "shortcodes, colon syntax, :smile:")),
    // InlineEmojiPicker
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Inline emoji picker"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Show an inline emoji picker when you type a colon (:) in the message input."),
      SM::TabComposer, composerInputInlineEmojiPickerEnabled, setComposerInputInlineEmojiPickerEnabled, nullptr, ComposerInputInlineEmojiPickerEnabled, SettingKey::ComposerInputInlineEmojiPickerEnabled),
    // PreferredEmojiGender
    withKeywords(
      SIMPLE_SEGMENTED_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Preferred gender"),
        QT_TRANSLATE_NOOP("UserSettingsModel",
          "Inline emoji picker only: prefer Unicode-defined gender variants (man/woman) when available. Searching explicitly for the other gender still works. <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/features/emojis.md#-emoji-preferences\">Learn more</a>."),
        SM::TabComposer, composerInputEmojiPreferredGender, setComposerInputEmojiPreferredGender, UserSettings::EmojiPreferredGender,
        composerEmojiPreferredGenderValues,
        nullptr,
        ComposerInputEmojiPreferredGender,
        SettingKey::ComposerInputEmojiPreferredGender),
      QT_TRANSLATE_NOOP("UserSettingsModel", "gender")),
    // PreferredEmojiSkinTone
    withKeywords(
      SIMPLE_OPTIONS_ENUM_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Preferred skin tone"),
        QT_TRANSLATE_NOOP("UserSettingsModel",
          "Inline emoji picker only: prefer one skin-tone variant when available. Searching explicitly for another skin tone still works. <a href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/features/emojis.md#-emoji-preferences\">Learn more</a>."),
        SM::TabComposer, composerInputEmojiPreferredSkinTone, setComposerInputEmojiPreferredSkinTone, UserSettings::EmojiPreferredSkinTone,
        composerEmojiPreferredSkinToneValues,
        nullptr,
        ComposerInputEmojiPreferredSkinTone,
        SettingKey::ComposerInputEmojiPreferredSkinTone),
      QT_TRANSLATE_NOOP("UserSettingsModel", "skin color, skin colour, skin")),
    // ComposerFeedbackSection
    { QT_TRANSLATE_NOOP("UserSettingsModel", "Feedback"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // ComposerTypingSendGlobal
    SIMPLE_BOOL_CONFIG_ID_SETTING(QT_TRANSLATE_NOOP("UserSettingsModel", "Show others when I'm typing"),
      QT_TRANSLATE_NOOP("UserSettingsModel", "Sends a typing notification while you compose, so others can see that a reply is on its way. Can be overridden per room in Room Info → Preferences."),
      SM::TabComposer, composerTypingSendEnabled, setComposerTypingSendEnabled, nullptr, ComposerTypingSendGlobal, SettingKey::ComposerTypingSendGlobal),
