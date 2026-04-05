// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "settings/ui/SettingRoleData.h"

#include <QCoreApplication>

#include "settings/core/SettingsDefinitions.h"
#include "settings/ui/SettingDescriptor.h"
#include "settings/ui/UserSettingsModel.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/KomaiGlobalObject.h"
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
        return ThemeRegistry::instance().themeVariant(i->uiThemeSlug()) == u"dark" ? 1 : 0;
    }

    if (role == UserSettingsModel::ThemeVariantValues) {
        return QStringList{
          QCoreApplication::translate("UserSettingsModel", "Light"),
          QCoreApplication::translate("UserSettingsModel", "Dark"),
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
    if (variantIdx < 0 || variantIdx > 1)
        return false;

    QString newVariant;
    if (variantIdx == 0)
        newVariant = QStringLiteral("light");
    else
        newVariant = QStringLiteral("dark");

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
autoplayGifVideosDescriptionRoleData(int role)
{
    if (role != UserSettingsModel::Description)
        return {};

    constexpr auto maxSizeMB    = Komai::kGifVideoMaxSizeBytes / (1024 * 1024);
    constexpr auto maxDurationS = Komai::kGifVideoMaxDurationMs / 1000;

    return QCoreApplication::translate(
             "UserSettingsModel",
             "Plays small video clips (under %1 MB or %2 s) inline, muted and looped. "
             "<a "
             "href=\"https://github.com/etkecc/komai/blob/main/docs/user-guide/"
             "media-playback.md#%EF%B8%8F-inline-gif-video-playback\">"
             "Learn more</a>.")
      .arg(maxSizeMB)
      .arg(maxDurationS);
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
    case settings::core::SettingId::TimelineMediaAutoplayGifVideos:
        return autoplayGifVideosDescriptionRoleData(role);
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
