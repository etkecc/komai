// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

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
    values << QCoreApplication::translate("UserSettingsModel", "Minimal")
           << QCoreApplication::translate("UserSettingsModel", "Bubbles");
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
