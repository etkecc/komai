// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
notificationsMessageContentPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Never")
           << QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms")
           << QCoreApplication::translate("UserSettingsModel", "Whenever available");
    return values;
}
