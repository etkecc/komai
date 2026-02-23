// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
sidebarsLastMessagePreviewValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static QVariant
sidebarsRoomSortValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Unread first, then recent")
           << QCoreApplication::translate("UserSettingsModel", "Unread first, then A-Z")
           << QCoreApplication::translate("UserSettingsModel", "Recent activity")
           << QCoreApplication::translate("UserSettingsModel", "Alphabetical");
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
timelineMessageLayoutValues()
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
timelineMessageActionsPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "On message hover")
           << QCoreApplication::translate("UserSettingsModel", "On button click")
           << QCoreApplication::translate("UserSettingsModel", "On message long press")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

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

static QVariant
notificationsMessageContentPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Never")
           << QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms")
           << QCoreApplication::translate("UserSettingsModel", "Whenever available");
    return values;
}
