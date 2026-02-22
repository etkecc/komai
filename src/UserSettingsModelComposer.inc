    // ── Composer Tab ────────────────────────────────────────────────────────

    // ComposerInputSection
    { QT_TR_NOOP("INPUT"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // Markdown
    { QT_TR_NOOP("Send messages as <a href=\"https://commonmark.org/help/\">Markdown</a>"),
      QT_TR_NOOP("Allow using markdown in messages.\nWhen disabled, all messages are sent as a plain text."),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->markdown(); },
      [](const QVariant &v) -> bool {
          bool enabled = false;
          if (!readSettingValue(v, enabled))
              return false;
          I->setMarkdown(enabled);
          return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // SendMessageKey
    { QT_TR_NOOP("Send messages with a shortcut"),
      QT_TR_NOOP("Select what Enter key combination sends the message. Shift+Enter adds a new line, unless it has been selected, in which case Enter adds a new line instead.\n\nIf an emoji picker or a mention picker is open, it is always handled first."),
      SM::Options, SM::TabComposer,
      []() -> QVariant { return static_cast<int>(I->sendMessageKey()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::SendMessageKey>().keyCount() <= val) return false;
          I->setSendMessageKey(static_cast<UserSettings::SendMessageKey>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Enter"),
            QCoreApplication::translate("UserSettingsModel", "Shift+Enter"),
            QCoreApplication::translate("UserSettingsModel", "Ctrl+Enter"),
          };
      },
      nullptr },
    // AutoReplaceEmoji
    { QT_TR_NOOP("Auto-replace text emoticons with emoji"),
      QT_TR_NOOP("Automatically replace text emoticons like :) :D :P with their emoji equivalents when sending a message. Choose whether to replace everywhere or only at the end."),
      SM::Options, SM::TabComposer,
      []() -> QVariant { return static_cast<int>(I->autoReplaceEmoji()); },
      [](const QVariant &v) -> bool {
          auto val = v.toInt();
          if (val < 0 || QMetaEnum::fromType<UserSettings::AutoReplaceEmoji>().keyCount() <= val) return false;
          I->setAutoReplaceEmoji(static_cast<UserSettings::AutoReplaceEmoji>(val)); return true;
      },
      {}, {}, {},
      []() -> QVariant {
          return QStringList{
            QCoreApplication::translate("UserSettingsModel", "Always"),
            QCoreApplication::translate("UserSettingsModel", "Only at the end of messages"),
            QCoreApplication::translate("UserSettingsModel", "Never"),
          };
      },
      nullptr },
    // ComposerFeedbackSection
    { QT_TR_NOOP("FEEDBACK"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // TypingNotifications
    { QT_TR_NOOP("Typing notifications"),
      QT_TR_NOOP("Show who is typing in a room.\nThis will also enable or disable sending typing notifications to others."),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->typingNotifications(); },
      [](const QVariant &v) -> bool {
          bool enabled = false;
          if (!readSettingValue(v, enabled))
              return false;
          I->setTypingNotifications(enabled);
          return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ReadReceipts
    { QT_TR_NOOP("Read receipts"),
      QT_TR_NOOP("Show if your message was read.\nStatus is displayed next to timestamps.\nWarning: If your homeserver does not support this, your rooms will never be marked as read!"),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->readReceipts(); },
      [](const QVariant &v) -> bool {
          bool enabled = false;
          if (!readSettingValue(v, enabled))
              return false;
          I->setReadReceipts(enabled);
          return true;
      },
      {}, {}, {}, nullptr, nullptr },
    // ComposerExtrasSection
    { QT_TR_NOOP("EXTRAS"), nullptr, SM::SectionTitle, SM::TabComposer,
      nullptr, nullptr, {}, {}, {}, nullptr, nullptr },
    // EnableStickers
    { QT_TR_NOOP("Enable stickers"),
      QT_TR_NOOP("Show the sticker button in the message composer, allowing you to send stickers from custom sticker packs."),
      SM::Toggle, SM::TabComposer,
      []() -> QVariant { return I->enableStickers(); },
      [](const QVariant &v) -> bool {
          bool enabled = false;
          if (!readSettingValue(v, enabled))
              return false;
          I->setEnableStickers(enabled);
          return true;
      },
      {}, {}, {}, nullptr, nullptr },
