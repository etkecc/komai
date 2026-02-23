    // Composer
    CONNECT_SETTING_KEY(SettingKey::ComposerInputMarkdownEnabled, markdownChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::ComposerInputSendKey, sendMessageKeyChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::ComposerInputAutoReplaceEmoji, autoReplaceEmojiChanged, Value);
    CONNECT_SETTING_KEY(
      SettingKey::ComposerFeedbackTypingNotifications, typingNotificationsChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::ComposerFeedbackReadReceipts, readReceiptsChanged, Value);
    CONNECT_SETTING_KEY(SettingKey::ComposerExtrasStickersEnabled, enableStickersChanged, Value);
