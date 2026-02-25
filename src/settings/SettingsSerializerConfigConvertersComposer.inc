// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

constexpr std::array<std::pair<UserSettings::AutoReplaceEmoji, const char *>, 3>
  kAutoReplaceEmojiTokens{{
    {UserSettings::AutoReplaceEmoji::Always, "always"},
    {UserSettings::AutoReplaceEmoji::OnlyAtEnd, "only_at_end"},
    {UserSettings::AutoReplaceEmoji::Never, "never"},
  }};

constexpr std::array<std::pair<UserSettings::SendMessageKey, const char *>, 3>
  kSendMessageKeyTokens{{
    {UserSettings::SendMessageKey::Enter, "enter"},
    {UserSettings::SendMessageKey::ShiftEnter, "shift_enter"},
    {UserSettings::SendMessageKey::CtrlEnter, "ctrl_enter"},
  }};

QString
toStorageValue(UserSettings::AutoReplaceEmoji value)
{
    return valueToStorageToken(value, kAutoReplaceEmojiTokens, "always");
}

UserSettings::AutoReplaceEmoji
autoReplaceEmojiFromStorage(const QString &value, UserSettings::AutoReplaceEmoji fallback)
{
    return valueFromStorageToken(value, fallback, kAutoReplaceEmojiTokens);
}

QString
toStorageValue(UserSettings::SendMessageKey value)
{
    return valueToStorageToken(value, kSendMessageKeyTokens, "enter");
}

UserSettings::SendMessageKey
sendMessageKeyFromStorage(const QString &value, UserSettings::SendMessageKey fallback)
{
    return valueFromStorageToken(value, fallback, kSendMessageKeyTokens);
}
