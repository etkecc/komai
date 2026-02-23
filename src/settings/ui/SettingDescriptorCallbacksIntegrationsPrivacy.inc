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

    int access = 0;
    if (!readSettingValue(v, access))
        return false;
    if (access < IntegrationsDbusAccessNone || access > IntegrationsDbusAccessReadWrite)
        return false;

    i->setIntegrationsDbusApiAccess(access);
    return true;
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

    switch (i->presence()) {
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

    int option = 0;
    if (!readSettingValue(v, option))
        return false;

    switch (option) {
    case 0:
        i->setPresence(UserSettings::Presence::Online);
        return true;
    case 1:
        i->setPresence(UserSettings::Presence::Unavailable);
        return true;
    case 2:
        i->setPresence(UserSettings::Presence::Offline);
        return true;
    case 3:
        i->setPresence(UserSettings::Presence::AutomaticPresence);
        return true;
    default:
        return false;
    }
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
privacyScreenTimeoutEnabled()
{
    auto i = I;
    return i && i->privacyScreen();
}
