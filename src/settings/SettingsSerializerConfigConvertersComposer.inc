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

constexpr std::array<std::pair<UserSettings::EmojiPreferredGender, const char *>, 3>
  kEmojiPreferredGenderTokens{{
    {UserSettings::EmojiPreferredGender::NoPreference, "no_preference"},
    {UserSettings::EmojiPreferredGender::Man, "man"},
    {UserSettings::EmojiPreferredGender::Woman, "woman"},
  }};

constexpr std::array<std::pair<UserSettings::EmojiPreferredSkinTone, const char *>, 6>
  kEmojiPreferredSkinToneTokens{{
    {UserSettings::EmojiPreferredSkinTone::NoPreference, "no_preference"},
    {UserSettings::EmojiPreferredSkinTone::Light, "light"},
    {UserSettings::EmojiPreferredSkinTone::MediumLight, "medium_light"},
    {UserSettings::EmojiPreferredSkinTone::Medium, "medium"},
    {UserSettings::EmojiPreferredSkinTone::MediumDark, "medium_dark"},
    {UserSettings::EmojiPreferredSkinTone::Dark, "dark"},
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
toStorageValue(UserSettings::EmojiPreferredGender value)
{
    return valueToStorageToken(value, kEmojiPreferredGenderTokens, "no_preference");
}

UserSettings::EmojiPreferredGender
emojiPreferredGenderFromStorage(const QString &value, UserSettings::EmojiPreferredGender fallback)
{
    return valueFromStorageToken(value, fallback, kEmojiPreferredGenderTokens);
}

QString
toStorageValue(UserSettings::EmojiPreferredSkinTone value)
{
    return valueToStorageToken(value, kEmojiPreferredSkinToneTokens, "no_preference");
}

UserSettings::EmojiPreferredSkinTone
emojiPreferredSkinToneFromStorage(const QString &value,
                                  UserSettings::EmojiPreferredSkinTone fallback)
{
    return valueFromStorageToken(value, fallback, kEmojiPreferredSkinToneTokens);
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
