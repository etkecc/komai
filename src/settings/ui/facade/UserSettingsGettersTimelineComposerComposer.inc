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

QString
UserSettings::composerInputSendKeyLabel() const
{
    // Strings live in the UserSettingsModel context so they share translations
    // with the settings-screen dropdown (composerSendMessageKeyValues()).
    switch (composerInputSendKey()) {
    case SendMessageKey::Enter:
        return QCoreApplication::translate("UserSettingsModel", "Enter");
    case SendMessageKey::ShiftEnter:
        return QCoreApplication::translate("UserSettingsModel", "Shift+Enter");
    case SendMessageKey::CtrlEnter:
        return QCoreApplication::translate("UserSettingsModel", "Ctrl+Enter");
    }
    return QCoreApplication::translate("UserSettingsModel", "Enter");
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
          coreStore_.valueAs<bool>(settings::core::SettingId::ComposerTypingSendGlobal);
        value.has_value())
        return *value;
    return composerTypingSendEnabled_;
}

QMap<QString, bool>
UserSettings::composerTypingSendEnabledByRoom() const
{
    return composerTypingSendEnabledByRoom_;
}

void
UserSettings::setComposerTypingSendEnabledByRoom(const QMap<QString, bool> &byRoom)
{
    composerTypingSendEnabledByRoom_ = byRoom;
    emit composerTypingSendEnabledByRoomChanged();
    save();
}

void
UserSettings::setComposerTypingSendEnabledForRoom(const QString &roomId, bool value)
{
    composerTypingSendEnabledByRoom_[roomId] = value;
    emit composerTypingSendEnabledByRoomChanged();
    save();
}

void
UserSettings::removeComposerTypingSendEnabledForRoom(const QString &roomId)
{
    if (composerTypingSendEnabledByRoom_.remove(roomId)) {
        emit composerTypingSendEnabledByRoomChanged();
        save();
    }
}

QVariant
UserSettings::composerTypingSendEnabledOverrideForRoom(const QString &roomId) const
{
    auto it = composerTypingSendEnabledByRoom_.find(roomId);
    if (it != composerTypingSendEnabledByRoom_.end())
        return QVariant(it.value());
    return QVariant();
}

bool
UserSettings::resolvedComposerTypingSendEnabled(const QString &roomId) const
{
    auto it = composerTypingSendEnabledByRoom_.find(roomId);
    if (it != composerTypingSendEnabledByRoom_.end())
        return it.value();
    return composerTypingSendEnabled();
}
