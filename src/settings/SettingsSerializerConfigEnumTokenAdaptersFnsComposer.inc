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
