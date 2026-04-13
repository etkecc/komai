// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static bool
navigationCommunitiesVisibleEnabled()
{
    return I->navigationCommunitiesVisible();
}

static QVariant
navigationLastMessagePreviewValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static QVariant
navigationRoomSortValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Unread first, then recent")
           << QCoreApplication::translate("UserSettingsModel", "Unread first, then A-Z")
           << QCoreApplication::translate("UserSettingsModel", "Recent activity")
           << QCoreApplication::translate("UserSettingsModel", "Alphabetical");
    return values;
}

static QVariant
navigationUnreadDetectionPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Any")
           << QCoreApplication::translate("UserSettingsModel", "Messages only");
    return values;
}

static QVariant
navigationTabsShowPinButtonValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static QVariant
navigationTabsPinnedTabLabelValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Avatar and label")
           << QCoreApplication::translate("UserSettingsModel", "Avatar only");
    return values;
}

static QVariant
navigationTabsTabLabelValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Avatar and label")
           << QCoreApplication::translate("UserSettingsModel", "Avatar only");
    return values;
}
