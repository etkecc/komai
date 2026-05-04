// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Source-English option lists for cross-locale settings search. See
// SettingDescriptorCallbacksLookFeelTheme.inc header for the rationale.
static const char *const navigationLastMessagePreviewValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Always"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Only in unencrypted rooms"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Never"),
  nullptr,
};

static QVariant
navigationLastMessagePreviewValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static const char *const navigationRoomSortValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Unread first, then recent"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Unread first, then A-Z"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Recent activity"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Alphabetical"),
  nullptr,
};

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

static const char *const navigationRoomListOpeningPolicyValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Reuse active tab"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Open new tab"),
  nullptr,
};

static QVariant
navigationRoomListOpeningPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Reuse active tab")
           << QCoreApplication::translate("UserSettingsModel", "Open new tab");
    return values;
}

static const char *const navigationTabsShowPinButtonValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Always"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Never"),
  nullptr,
};

static QVariant
navigationTabsShowPinButtonValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Never");
    return values;
}

static const char *const navigationTabsPinnedTabLabelValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Avatar and label"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Avatar only"),
  nullptr,
};

static QVariant
navigationTabsPinnedTabLabelValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Avatar and label")
           << QCoreApplication::translate("UserSettingsModel", "Avatar only");
    return values;
}

// Same set as pinned-tab; declared separately so the registry can map by
// helper-pointer identity.
static const char *const navigationTabsTabLabelValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Avatar and label"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Avatar only"),
  nullptr,
};

static QVariant
navigationTabsTabLabelValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Avatar and label")
           << QCoreApplication::translate("UserSettingsModel", "Avatar only");
    return values;
}
