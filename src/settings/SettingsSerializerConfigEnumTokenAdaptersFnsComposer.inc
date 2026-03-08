QString
autoReplaceEmojiToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.composerInputAutoReplaceEmoji());
}

void
applyAutoReplaceEmojiFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setComposerInputAutoReplaceEmoji(
      autoReplaceEmojiFromStorage(rawToken, UserSettings::AutoReplaceEmoji::Always));
}

QString
emojiPreferredGenderToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.composerInputEmojiPreferredGender());
}

void
applyEmojiPreferredGenderFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setComposerInputEmojiPreferredGender(emojiPreferredGenderFromStorage(
      rawToken, UserSettings::EmojiPreferredGender::NoPreference));
}

QString
emojiPreferredSkinToneToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.composerInputEmojiPreferredSkinTone());
}

void
applyEmojiPreferredSkinToneFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setComposerInputEmojiPreferredSkinTone(emojiPreferredSkinToneFromStorage(
      rawToken, UserSettings::EmojiPreferredSkinTone::NoPreference));
}

QString
sendMessageKeyToStorage(const UserSettings &settings)
{
    return toStorageValue(settings.composerInputSendKey());
}

void
applySendMessageKeyFromStorage(UserSettings &settings, const QString &rawToken)
{
    settings.setComposerInputSendKey(
      sendMessageKeyFromStorage(rawToken, UserSettings::SendMessageKey::Enter));
}
