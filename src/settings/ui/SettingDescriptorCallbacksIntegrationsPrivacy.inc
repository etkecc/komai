// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
integrationsDbusApiAccessValue()
{
    auto i = I;
    if (!i)
        return IntegrationsDbusAccessNone;
    return i->integrationsDbusApiAccess();
}

static bool
setIntegrationsDbusApiAccessValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyIndexedOption(
      v, IntegrationsDbusAccessNone, IntegrationsDbusAccessReadWrite, [i](int access) {
          i->setIntegrationsDbusApiAccess(access);
      });
}

static QVariant
integrationsDbusApiAccessValues()
{
    return QStringList{
      QCoreApplication::translate("UserSettingsModel", "None"),
      QCoreApplication::translate("UserSettingsModel", "Read-only"),
      QCoreApplication::translate("UserSettingsModel", "Read & write"),
    };
}

static QVariant
privacyPresenceStatusPolicyValue()
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
setPrivacyPresenceStatusPolicyValue(const QVariant &v)
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

static QVariant
privacyPresenceStatusPolicyValues()
{
    return QStringList{
      QCoreApplication::translate("UserSettingsModel", "Always online"),
      QCoreApplication::translate("UserSettingsModel", "Always unavailable"),
      QCoreApplication::translate("UserSettingsModel", "Always offline"),
      QCoreApplication::translate("UserSettingsModel", "Automatic"),
    };
}

static bool
privacyWindowFocusBlurDelaySecondsEnabled()
{
    auto i = I;
    return i && i->privacyWindowFocusBlurEnabled();
}
