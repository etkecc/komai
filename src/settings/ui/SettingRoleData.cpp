// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SettingRoleData.h"

#include <QCoreApplication>

#include <mtx/secret_storage.hpp>

#include "Cache.h"
#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/UserSettingsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/ThemeRegistry.h"

namespace settings::ui {

namespace {

QVariant
themeRoleData(int role)
{
    auto i = UserSettings::instance();
    if (!i)
        return {};

    if (role == UserSettingsModel::ThemeVariantValue) {
        const auto variant = ThemeRegistry::instance().themeVariant(i->uiThemeSlug());
        if (variant == u"light")
            return 0;
        if (variant == u"dark")
            return 1;
        return 2;
    }

    if (role == UserSettingsModel::ThemeVariantValues) {
        return QStringList{
          QCoreApplication::translate("UserSettingsModel", "Light"),
          QCoreApplication::translate("UserSettingsModel", "Dark"),
          QCoreApplication::translate("UserSettingsModel", "System"),
        };
    }

    return {};
}

bool
setThemeRoleData(int role, const QVariant &value)
{
    if (role != UserSettingsModel::ThemeVariantValue)
        return false;

    auto i = UserSettings::instance();
    if (!i)
        return false;

    int variantIdx = 0;
    if (!readSettingValue(value, variantIdx))
        return false;
    if (variantIdx < 0 || variantIdx > 2)
        return false;

    QString newVariant;
    if (variantIdx == 0)
        newVariant = QStringLiteral("light");
    else if (variantIdx == 1)
        newVariant = QStringLiteral("dark");
    else
        newVariant = QStringLiteral("system");

    const auto currentVariant = ThemeRegistry::instance().themeVariant(i->uiThemeSlug());
    if (newVariant == currentVariant)
        return false;

    i->setUiThemeSlug(ThemeRegistry::instance().defaultThemeSlug(newVariant));
    return true;
}

QVariant
presenceStatusDescriptionRoleData(int role)
{
    if (role != UserSettingsModel::Description)
        return {};

    return QCoreApplication::translate(
             "UserSettingsModel",
             "Controls your <a href=\"%1\">Presence</a> status on the Matrix network.\n"
             "Automatic is either 'online' or 'unavailable' (after 5 minutes of inactivity).")
      .arg(QStringLiteral("https://spec.matrix.org/v1.17/client-server-api/#presence"));
}

QVariant
uiLayoutContentMaxWidthDescriptionRoleData(int role)
{
    if (role != UserSettingsModel::Description)
        return {};

    return QCoreApplication::translate(
             "UserSettingsModel",
             "Set the maximum width (in pixels) for app content, including timeline messages. "
             "Use 0 for uncapped; minimum effective value is %1.")
      .arg(settings::core::definitions::kMinEffectiveUiLayoutContentMaxWidthPx);
}

QVariant
keyStatusRoleData(int role, bool good)
{
    if (role == UserSettingsModel::Good)
        return good;

    return {};
}

} // namespace

QVariant
roleDataForSetting(settings::core::SettingId id, int role)
{
    switch (id) {
    case settings::core::SettingId::UiThemeSlug:
        return themeRoleData(role);
    case settings::core::SettingId::UiLayoutContentMaxWidthPx:
        return uiLayoutContentMaxWidthDescriptionRoleData(role);
    case settings::core::SettingId::NetworkPresenceStatusPolicy:
        return presenceStatusDescriptionRoleData(role);
    case settings::core::SettingId::EncryptionOnlineBackupKeyStatus:
        return keyStatusRoleData(
          role, cache::secret(mtx::secret_storage::secrets::megolm_backup_v1).has_value());
    case settings::core::SettingId::EncryptionSelfSigningKeyStatus:
        return keyStatusRoleData(
          role,
          cache::secret(mtx::secret_storage::secrets::cross_signing_self_signing).has_value());
    case settings::core::SettingId::EncryptionUserSigningKeyStatus:
        return keyStatusRoleData(
          role,
          cache::secret(mtx::secret_storage::secrets::cross_signing_user_signing).has_value());
    case settings::core::SettingId::EncryptionMasterSigningKeyStatus:
        return keyStatusRoleData(role, true);
    default:
        return {};
    }
}

bool
hasWritableRoleDataForSetting(settings::core::SettingId id, int role)
{
    return id == settings::core::SettingId::UiThemeSlug &&
           role == UserSettingsModel::ThemeVariantValue;
}

bool
setRoleDataForSetting(settings::core::SettingId id, int role, const QVariant &value)
{
    if (id == settings::core::SettingId::UiThemeSlug)
        return setThemeRoleData(role, value);

    (void)role;
    (void)value;
    return false;
}

} // namespace settings::ui
