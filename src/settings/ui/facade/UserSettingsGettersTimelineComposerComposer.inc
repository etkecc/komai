bool
UserSettings::composerInputMarkdownToHtmlEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerInputMarkdownToHtmlEnabled);
        value.has_value())
        return *value;
    return composerInputMarkdownToHtmlEnabled_;
}

UserSettings::SendMessageKey
UserSettings::composerInputSendKey() const
{
    if (const auto value = coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputSendKey);
        value.has_value() && *value >= static_cast<int>(UserSettings::SendMessageKey::Enter) &&
        *value <= static_cast<int>(UserSettings::SendMessageKey::CtrlEnter))
        return static_cast<UserSettings::SendMessageKey>(*value);
    return composerInputSendKey_;
}

UserSettings::AutoReplaceEmoji
UserSettings::composerInputAutoReplaceEmoji() const
{
    if (const auto value =
          coreStore_.valueAs<int>(settings::core::SettingId::ComposerInputAutoReplaceEmoji);
        value.has_value() && *value >= static_cast<int>(UserSettings::AutoReplaceEmoji::Always) &&
        *value <= static_cast<int>(UserSettings::AutoReplaceEmoji::Never))
        return static_cast<UserSettings::AutoReplaceEmoji>(*value);
    return composerInputAutoReplaceEmoji_;
}

UserSettings::EmojiPreferredGender
UserSettings::composerInputEmojiPreferredGender() const
{
    if (const auto value = coreStore_.valueAs<int>(
          settings::core::SettingId::ComposerInputEmojiPreferredGender);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::EmojiPreferredGender::NoPreference) &&
        *value <= static_cast<int>(UserSettings::EmojiPreferredGender::Woman))
        return static_cast<UserSettings::EmojiPreferredGender>(*value);
    return composerInputEmojiPreferredGender_;
}

UserSettings::EmojiPreferredSkinTone
UserSettings::composerInputEmojiPreferredSkinTone() const
{
    if (const auto value = coreStore_.valueAs<int>(
          settings::core::SettingId::ComposerInputEmojiPreferredSkinTone);
        value.has_value() &&
        *value >= static_cast<int>(UserSettings::EmojiPreferredSkinTone::NoPreference) &&
        *value <= static_cast<int>(UserSettings::EmojiPreferredSkinTone::Dark))
        return static_cast<UserSettings::EmojiPreferredSkinTone>(*value);
    return composerInputEmojiPreferredSkinTone_;
}

bool
UserSettings::composerInputInlineEmojiPickerEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::ComposerInputInlineEmojiPickerEnabled);
        value.has_value())
        return *value;
    return composerInputInlineEmojiPickerEnabled_;
}

bool
UserSettings::composerInputInlineRoomPickerEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::ComposerInputInlineRoomPickerEnabled);
        value.has_value())
        return *value;
    return composerInputInlineRoomPickerEnabled_;
}

bool
UserSettings::composerInputInlineUserPickerEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::ComposerInputInlineUserPickerEnabled);
        value.has_value())
        return *value;
    return composerInputInlineUserPickerEnabled_;
}

bool
UserSettings::composerInputTranscriptionEnabled() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::ComposerInputTranscriptionEnabled);
        value.has_value())
        return *value;
    return composerInputTranscriptionEnabled_;
}

bool
UserSettings::composerAttachmentsStripImageMetadata() const
{
    if (const auto value = coreStore_.valueAs<bool>(
          settings::core::SettingId::ComposerAttachmentsStripImageMetadata);
        value.has_value())
        return *value;
    return composerAttachmentsStripImageMetadata_;
}

bool
UserSettings::composerTypingSendEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerTypingSendEnabled);
        value.has_value())
        return *value;
    return composerTypingSendEnabled_;
}
