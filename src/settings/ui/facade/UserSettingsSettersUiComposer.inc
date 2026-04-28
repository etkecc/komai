namespace {
QString
providerPreferredGender(UserSettings::EmojiPreferredGender gender)
{
    switch (gender) {
    case UserSettings::EmojiPreferredGender::Man:
        return QStringLiteral("man");
    case UserSettings::EmojiPreferredGender::Woman:
        return QStringLiteral("woman");
    case UserSettings::EmojiPreferredGender::NoPreference:
        return {};
    }

    return {};
}

QString
providerPreferredSkinToneClass(UserSettings::EmojiPreferredSkinTone skinTone)
{
    switch (skinTone) {
    case UserSettings::EmojiPreferredSkinTone::Light:
        return QStringLiteral("single_light");
    case UserSettings::EmojiPreferredSkinTone::MediumLight:
        return QStringLiteral("single_medium_light");
    case UserSettings::EmojiPreferredSkinTone::Medium:
        return QStringLiteral("single_medium");
    case UserSettings::EmojiPreferredSkinTone::MediumDark:
        return QStringLiteral("single_medium_dark");
    case UserSettings::EmojiPreferredSkinTone::Dark:
        return QStringLiteral("single_dark");
    case UserSettings::EmojiPreferredSkinTone::NoPreference:
        return {};
    }

    return {};
}
} // namespace

void
UserSettings::setComposerInputMarkdownToHtmlEnabled(bool s)
{
    if (composerInputMarkdownToHtmlEnabled_ == s)
        return;
    composerInputMarkdownToHtmlEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputMarkdownToHtmlEnabled, s);
    emit composerInputMarkdownToHtmlEnabledChanged(s);
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
UserSettings::setComposerInputEmojiPreferredGender(EmojiPreferredGender s)
{
    emoji::Provider::setPreferredGender(providerPreferredGender(s));

    if (composerInputEmojiPreferredGender_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::ComposerInputEmojiPreferredGender,
                      static_cast<int>(s),
                      "composer.input.emoji.preferred_gender"))
        return;
    composerInputEmojiPreferredGender_ = s;
    emit composerInputEmojiPreferredGenderChanged(s);
    save();
}
void
UserSettings::setComposerInputEmojiPreferredSkinTone(EmojiPreferredSkinTone s)
{
    emoji::Provider::setPreferredSkinToneClass(providerPreferredSkinToneClass(s));

    if (composerInputEmojiPreferredSkinTone_ == s)
        return;
    if (!setCoreValue(settings::core::SettingId::ComposerInputEmojiPreferredSkinTone,
                      static_cast<int>(s),
                      "composer.input.emoji.preferred_skin_tone"))
        return;
    composerInputEmojiPreferredSkinTone_ = s;
    emit composerInputEmojiPreferredSkinToneChanged(s);
    save();
}
void
UserSettings::setComposerInputInlineEmojiPickerEnabled(bool s)
{
    if (composerInputInlineEmojiPickerEnabled_ == s)
        return;
    composerInputInlineEmojiPickerEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputInlineEmojiPickerEnabled, s);
    emit composerInputInlineEmojiPickerEnabledChanged(s);
    save();
}
void
UserSettings::setComposerInputInlineRoomPickerEnabled(bool s)
{
    if (composerInputInlineRoomPickerEnabled_ == s)
        return;
    composerInputInlineRoomPickerEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputInlineRoomPickerEnabled, s);
    emit composerInputInlineRoomPickerEnabledChanged(s);
    save();
}
void
UserSettings::setComposerInputInlineUserPickerEnabled(bool s)
{
    if (composerInputInlineUserPickerEnabled_ == s)
        return;
    composerInputInlineUserPickerEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputInlineUserPickerEnabled, s);
    emit composerInputInlineUserPickerEnabledChanged(s);
    save();
}
void
UserSettings::setComposerInputTranscriptionEnabled(bool s)
{
    if (composerInputTranscriptionEnabled_ == s)
        return;
    composerInputTranscriptionEnabled_ = s;
    (void)coreStore_.set(settings::core::SettingId::ComposerInputTranscriptionEnabled, s);
    emit composerInputTranscriptionEnabledChanged(s);
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
