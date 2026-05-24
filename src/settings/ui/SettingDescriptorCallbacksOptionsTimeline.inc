// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Source-English option lists for cross-locale settings search. See
// SettingDescriptorCallbacksLookFeelTheme.inc header for the rationale.
static const char *const timelineAvatarSizeValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Regular"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Small"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Hidden"),
  nullptr,
};

static QVariant
timelineAvatarSizeValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Regular")
           << QCoreApplication::translate("UserSettingsModel", "Small")
           << QCoreApplication::translate("UserSettingsModel", "Hidden");
    return values;
}

static const char *const timelineSenderUsernameValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Always"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Only in large rooms (> 16 members)"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Never"),
  nullptr,
};

static QVariant
timelineSenderUsernameValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in large rooms (> 16 members)")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static const char *const timelineMessagesStyleValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Plain"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Bubbles"),
  nullptr,
};

static QVariant
timelineMessagesStyleValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Plain")
           << QCoreApplication::translate("UserSettingsModel", "Bubbles");
    return values;
}

static const char *const timelineRoomHeaderButtonLabelsValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Adaptive"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Never"),
  nullptr,
};

static QVariant
timelineRoomHeaderButtonLabelsValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Adaptive")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static const char *const timelineMessagesLayoutPositioningValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Adaptive"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Opposing by sender"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "All left-aligned"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "All right-aligned"),
  nullptr,
};

static QVariant
timelineMessagesLayoutPositioningValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Adaptive")
           << QCoreApplication::translate("UserSettingsModel", "Opposing by sender")
           << QCoreApplication::translate("UserSettingsModel", "All left-aligned")
           << QCoreApplication::translate("UserSettingsModel", "All right-aligned");
    return values;
}

static const char *const timelineUserColorCodingPolicyValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Adaptive (per-member in small rooms)"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Me vs others"),
  nullptr,
};

static QVariant
timelineUserColorCodingPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel",
                                          "Adaptive (per-member in small rooms)")
           << QCoreApplication::translate("UserSettingsModel", "Me vs others");
    return values;
}

static const char *const timelineShowImageValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Always"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Only in private rooms"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Never"),
  nullptr,
};

static QVariant
timelineShowImageValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in private rooms")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static const char *const timelineMessageActionsActivationPolicyValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "On message hover"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "On button click"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Never"),
  nullptr,
};

static QVariant
timelineMessageActionsActivationPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "On message hover")
           << QCoreApplication::translate("UserSettingsModel", "On button click")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}
