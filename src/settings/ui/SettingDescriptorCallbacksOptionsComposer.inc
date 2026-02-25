// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
composerSendMessageKeyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Enter")
           << QCoreApplication::translate("UserSettingsModel", "Shift+Enter")
           << QCoreApplication::translate("UserSettingsModel", "Ctrl+Enter");
    return values;
}

static QVariant
composerAutoReplaceEmojiValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only at the end of messages")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}
