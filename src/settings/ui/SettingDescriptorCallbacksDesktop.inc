// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Source-English option list for cross-locale settings search. See
// SettingDescriptorCallbacksLookFeelTheme.inc header for the rationale.
static const char *const desktopNotificationsMessageContentPolicyValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Never"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Only in unencrypted rooms"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Whenever available"),
  nullptr,
};

static QVariant
desktopNotificationsMessageContentPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Never")
           << QCoreApplication::translate("UserSettingsModel", "Only in unencrypted rooms")
           << QCoreApplication::translate("UserSettingsModel", "Whenever available");
    return values;
}

static QVariant
desktopNotificationsAccountEnabledValue()
{
    if (const auto settings = UserSettings::instance()) {
        settings->refreshNotificationsAccountEnabled();
        return settings->notificationsAccountEnabled();
    }

    return true;
}

static bool
setDesktopNotificationsAccountEnabledValue(const QVariant &value)
{
    bool enabled = true;
    if (!readSettingValue(value, enabled))
        return false;

    if (const auto settings = UserSettings::instance()) {
        settings->setNotificationsAccountEnabled(enabled);
        return true;
    }

    return false;
}

static bool
desktopNotificationsAccountToggleEnabled()
{
    if (const auto settings = UserSettings::instance())
        return settings->notificationsAccountToggleEnabled();

    return false;
}

static bool
desktopNotificationsSettingsEnabled()
{
    if (const auto settings = UserSettings::instance()) {
        if (!settings->hasActiveSession())
            return true;

        return settings->notificationsAccountEnabled();
    }

    return false;
}

static bool
desktopWindowFocusBlurDelaySecondsEnabled()
{
    auto i = I;
    return i && i->desktopWindowFocusBlurEnabled();
}
