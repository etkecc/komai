// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
networkPresenceStatusPolicyValue()
{
    auto i = I;
    if (!i)
        return 3;

    switch (i->networkPresenceStatusPolicy()) {
    case UserSettings::Presence::Online:
        return 0;
    case UserSettings::Presence::Unavailable:
        return 1;
    case UserSettings::Presence::Offline:
        return 2;
    case UserSettings::Presence::AutomaticPresence:
        return 3;
    }

    return 3;
}

static bool
setNetworkPresenceStatusPolicyValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    static constexpr std::array<UserSettings::Presence, 4> kPresenceByIndex{
      UserSettings::Presence::Online,
      UserSettings::Presence::Unavailable,
      UserSettings::Presence::Offline,
      UserSettings::Presence::AutomaticPresence,
    };

    return applyIndexedOption(v, 0, static_cast<int>(kPresenceByIndex.size() - 1), [i](int option) {
        i->setNetworkPresenceStatusPolicy(kPresenceByIndex.at(static_cast<size_t>(option)));
    });
}

// Source-English option list for cross-locale settings search. See
// SettingDescriptorCallbacksLookFeelTheme.inc header for the rationale.
static const char *const networkPresenceStatusPolicyValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Always online"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Always unavailable"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Always offline"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Automatic"),
  nullptr,
};

static QVariant
networkPresenceStatusPolicyValues()
{
    return QStringList{
      QCoreApplication::translate("UserSettingsModel", "Always online"),
      QCoreApplication::translate("UserSettingsModel", "Always unavailable"),
      QCoreApplication::translate("UserSettingsModel", "Always offline"),
      QCoreApplication::translate("UserSettingsModel", "Automatic"),
    };
}
