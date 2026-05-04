// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Source-English option lists for cross-locale settings search. See
// SettingDescriptorCallbacksLookFeelTheme.inc header for the rationale.
static const char *const composerSendMessageKeyValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Enter"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Shift+Enter"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Ctrl+Enter"),
  nullptr,
};

static QVariant
composerSendMessageKeyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Enter")
           << QCoreApplication::translate("UserSettingsModel", "Shift+Enter")
           << QCoreApplication::translate("UserSettingsModel", "Ctrl+Enter");
    return values;
}

static const char *const composerAutoReplaceEmojiValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Always"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Only at the end of messages"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Never"),
  nullptr,
};

static QVariant
composerAutoReplaceEmojiValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only at the end of messages")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static const char *const composerEmojiPreferredGenderValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "No preference"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "👨 Man"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "👩 Woman"),
  nullptr,
};

static QVariant
composerEmojiPreferredGenderValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "No preference")
           << QCoreApplication::translate("UserSettingsModel", "👨 Man")
           << QCoreApplication::translate("UserSettingsModel", "👩 Woman");
    return values;
}

static const char *const composerEmojiPreferredSkinToneValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "No preference"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "👍🏻 Light"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "👍🏼 Medium-light"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "👍🏽 Medium"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "👍🏾 Medium-dark"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "👍🏿 Dark"),
  nullptr,
};

static QVariant
composerEmojiPreferredSkinToneValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "No preference")
           << QCoreApplication::translate("UserSettingsModel", "👍🏻 Light")
           << QCoreApplication::translate("UserSettingsModel", "👍🏼 Medium-light")
           << QCoreApplication::translate("UserSettingsModel", "👍🏽 Medium")
           << QCoreApplication::translate("UserSettingsModel", "👍🏾 Medium-dark")
           << QCoreApplication::translate("UserSettingsModel", "👍🏿 Dark");
    return values;
}
