// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

// Helper: convert std::vector<std::string> to QStringList
static QStringList
vecToList(const std::vector<std::string> &vec)
{
    QStringList l;
    for (const auto &d : vec)
        l.push_back(QString::fromStdString(d));
    return l;
}

static QVariant
lookFeelThemeValue()
{
    auto i = I;
    if (!i)
        return -1;

    auto variant = ThemeRegistry::instance().themeVariant(i->theme());
    if (variant == u"system")
        return -1;

    return ThemeRegistry::instance().themeSlugs(variant).indexOf(i->theme());
}

static bool
setLookFeelThemeValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    auto variant = ThemeRegistry::instance().themeVariant(i->theme());
    if (variant == u"system")
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    auto slugs = ThemeRegistry::instance().themeSlugs(variant);
    if (idx < 0 || idx >= slugs.size())
        return false;

    i->setTheme(slugs.at(idx));
    return true;
}

static QVariant
lookFeelThemeValues()
{
    auto i = I;
    if (!i)
        return QStringList{};

    auto variant = ThemeRegistry::instance().themeVariant(i->theme());
    if (variant == u"system")
        return QStringList{};

    return ThemeRegistry::instance().themeNames(variant);
}

static QVariant
lookFeelFontFamilyValue()
{
    auto i = I;
    if (!i)
        return 0;

    if (i->font().isEmpty())
        return 0;

    auto fonts = QFontDatabase::families();
    fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
    return fonts.indexOf(i->font());
}

static bool
setLookFeelFontFamilyValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    auto fonts = QFontDatabase::families();
    if (idx < 0 || idx > fonts.size())
        return false;

    i->setFontFamily(idx == 0 ? QString{} : fonts.at(idx - 1));
    return true;
}

static QVariant
lookFeelFontFamilyValues()
{
    auto fonts = QFontDatabase::families();
    fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System font"));
    return fonts;
}

static QVariant
lookFeelEmojiFontFamilyValue()
{
    auto i = I;
    if (!i)
        return 0;

    if (i->emojiFontFamily().isEmpty())
        return 0;

    auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
    fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
    return fonts.indexOf(i->emojiFontFamily());
}

static bool
setLookFeelEmojiFontFamilyValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    int idx = 0;
    if (!readSettingValue(v, idx))
        return false;

    auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
    if (idx < 0 || idx > fonts.size())
        return false;

    i->setEmojiFontFamily(idx <= 0 ? QString{} : fonts.at(idx - 1));
    return true;
}

static QVariant
lookFeelEmojiFontFamilyValues()
{
    auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
    fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
    return fonts;
}

static bool
identiconSettingEnabled()
{
    return JdenticonProvider::isAvailable();
}

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

#include "settings/ui/SettingDescriptorCallbacksCalls.inc"

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
timelineShowImageValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Always")
           << QCoreApplication::translate("UserSettingsModel", "Only in private rooms")
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
encryptionOnlineBackupKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value();
}

static QVariant
encryptionSelfSigningKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing).has_value();
}

static QVariant
encryptionUserSigningKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing).has_value();
}

static QVariant
encryptionMasterSigningKeyStatusValue()
{
    return cache::secret(mtx::secret_storage::secrets::cross_signing_master).has_value();
}

static QVariant
sessionProfileValue()
{
    auto i = I;
    if (!i)
        return QCoreApplication::translate("UserSettingsModel", "Default");

    return i->profile().isEmpty() ? QCoreApplication::translate("UserSettingsModel", "Default")
                                  : i->profile();
}

static QVariant
sessionDeviceFingerprintValue()
{
    auto fingerprint = utils::humanReadableFingerprint(olm::client()->identity_keys().ed25519);
    fingerprint.replace(u'\n', u' ');
    return fingerprint.simplified();
}

static QVariant
aboutAppNameValue()
{
    return QStringLiteral("<a href=\"https://github.com/etkecc/komai\">Komai</a> @ ") +
           QString::fromStdString(nheko::version) +
           QStringLiteral(" (<a href=\"https://github.com/etkecc/komai/commit/") +
           QString::fromStdString(nheko::commit_hash) + QStringLiteral("\">") +
           QString::fromStdString(nheko::commit_hash) + QStringLiteral("</a>)");
}

static QVariant
aboutPlatformValue()
{
    return QString::fromStdString(nheko::build_os);
}

static QVariant
aboutBasedOnValue()
{
    return QStringLiteral("<a href=\"https://nheko.im/nheko-reborn/nheko\">nheko</a> @ ~v0.12.1 "
                          "(<a href=\"https://nheko.im/nheko-reborn/nheko/-/commit/"
                          "abb2325a995f936081219f402339fc8e0a661ac1\">abb2325a</a>)");
}

static QVariant
aboutMaintainedByValue()
{
    return QStringLiteral("<a href=\"https://etke.cc\">etke.cc</a>");
}
