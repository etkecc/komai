bool
UserSettings::composerInputMarkdownEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerInputMarkdownEnabled);
        value.has_value())
        return *value;
    return composerInputMarkdownEnabled_;
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

bool
UserSettings::composerExtrasStickersEnabled() const
{
    if (const auto value =
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerExtrasStickersEnabled);
        value.has_value())
        return *value;
    return composerExtrasStickersEnabled_;
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
