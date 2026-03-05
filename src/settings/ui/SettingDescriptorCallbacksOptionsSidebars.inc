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
sidebarsUnreadDetectionPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Any")
           << QCoreApplication::translate("UserSettingsModel", "Messages only");
    return values;
}
