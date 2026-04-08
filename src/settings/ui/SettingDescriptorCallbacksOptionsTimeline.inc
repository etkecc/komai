// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
timelineAvatarSizeValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Regular")
           << QCoreApplication::translate("UserSettingsModel", "Small")
           << QCoreApplication::translate("UserSettingsModel", "Hidden");
    return values;
}

static QVariant
timelineSenderUsernameValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in large rooms (> 16 members)")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static QVariant
timelineMessagesStyleValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Plain")
           << QCoreApplication::translate("UserSettingsModel", "Bubbles");
    return values;
}

static QVariant
timelineMessagesLayoutPositioningValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Opposing by sender")
           << QCoreApplication::translate("UserSettingsModel", "All left-aligned")
           << QCoreApplication::translate("UserSettingsModel", "All right-aligned");
    return values;
}

static QVariant
timelineUserColorCodingPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel",
                                          "Adaptive (per-member in small rooms)")
           << QCoreApplication::translate("UserSettingsModel", "Me vs others");
    return values;
}

static QVariant
timelineShowImageValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in private rooms")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static QVariant
timelineMessageActionsActivationPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "On message hover")
           << QCoreApplication::translate("UserSettingsModel", "On button click")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}
