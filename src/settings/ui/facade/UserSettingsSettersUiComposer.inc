void
UserSettings::setComposerInputMarkdownEnabled(bool s)
{
    if (composerInputMarkdownEnabled_ == s)
        return;
    composerInputMarkdownEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputMarkdownEnabled, s);
    emit composerInputMarkdownEnabledChanged(s);
    save();
}
void
UserSettings::setComposerInputSendKey(SendMessageKey s)
{
    if (composerInputSendKey_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::ComposerInputSendKey,
                      static_cast<int>(s),
                      "composer.input.send_key"))
        return;
    composerInputSendKey_ = s;
    emit composerInputSendKeyChanged(s);
    save();
}
void
UserSettings::setComposerInputAutoReplaceEmoji(AutoReplaceEmoji s)
{
    if (composerInputAutoReplaceEmoji_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::ComposerInputAutoReplaceEmoji,
                      static_cast<int>(s),
                      "composer.input.auto_replace_emoji"))
        return;
    composerInputAutoReplaceEmoji_ = s;
    emit composerInputAutoReplaceEmojiChanged(s);
    save();
}
void
UserSettings::setComposerExtrasStickersEnabled(bool s)
{
    if (composerExtrasStickersEnabled_ == s)
        return;
    composerExtrasStickersEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerExtrasStickersEnabled, s);
    emit composerExtrasStickersEnabledChanged(s);
    save();
}
void
UserSettings::setComposerTypingSendEnabled(bool s)
{
    if (composerTypingSendEnabled_ == s)
        return;
    composerTypingSendEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerTypingSendEnabled, s);
    emit composerTypingSendEnabledChanged(s);
    save();
}
