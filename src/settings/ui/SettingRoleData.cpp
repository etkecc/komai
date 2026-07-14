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
        if (i->uiThemeMode() == UserSettings::ThemeMode::Auto)
            return 2;
        return ThemeRegistry::instance().themeVariant(i->uiThemeSlug()) == u"dark" ? 1 : 0;
    }

    if (role == UserSettingsModel::ThemeVariantValues) {
        return QStringList{
          QCoreApplication::translate("UserSettingsModel", "Light"),
          QCoreApplication::translate("UserSettingsModel", "Dark"),
          QCoreApplication::translate("UserSettingsModel", "Auto"),
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

    // Auto: only the mode changes; applyOsColorScheme resolves the effective
    // slug at runtime without persisting it.
    if (variantIdx == 2) {
        if (i->uiThemeMode() == UserSettings::ThemeMode::Auto)
            return false;
        i->setUiThemeMode(UserSettings::ThemeMode::Auto);
        i->applyOsColorScheme();
        return true;
    }

    // Light/Dark: an explicit choice pins both the mode and the slug's variant.
    const auto mode = variantIdx == 0 ? UserSettings::ThemeMode::Light : UserSettings::ThemeMode::Dark;
    const QString newVariant = variantIdx == 0 ? QStringLiteral("light") : QStringLiteral("dark");
    if (i->uiThemeMode() == mode &&
        ThemeRegistry::instance().themeVariant(i->uiThemeSlug()) == newVariant)
        return false;

    i->setUiThemeMode(mode);
    i->setThemeVariantByIndex(variantIdx);
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
             "features/media-playback.md#%EF%B8%8F-inline-gif-video-playback\">"
             "Learn more</a>.")
      .arg(maxSizeMB)
      .arg(maxDurationS);
}

QVariant
senderUsernameDescriptionRoleData(int role)
{
    if (role != UserSettingsModel::Description)
        return {};

    auto i = UserSettings::instance();
    const bool avatarsHidden =
      i && i->timelineMessagesLayoutAvatarSize() == UserSettings::AvatarSize::Hidden;

    auto base = QCoreApplication::translate(
      "UserSettingsModel",
      "Control when sender usernames are displayed above messages. In bubble mode, your own "
      "username is always hidden. In smaller rooms, avatars and bubble colors are often enough "
      "context.");

    if (avatarsHidden) {
        QString warningColor;
        if (const auto *theme = ThemeRegistry::instance().findTheme(i->uiThemeSlug()))
            warningColor = theme->attention.name();

        const auto warning = QCoreApplication::translate(
          "UserSettingsModel",
          "⚠ Avatar size is set to Hidden, so sender usernames are always shown.");

        if (!warningColor.isEmpty())
            base += QStringLiteral("<br><br><span style=\"color:%1\">%2</span>")
                      .arg(warningColor, warning);
        else
            base += QStringLiteral("<br><br>") + warning;
    }

    return base;
}

} // namespace

QVariant
roleDataForSetting(settings::core::SettingId id, int role)
{
    switch (id) {
    case settings::core::SettingId::UiThemeSlug:
        return themeRoleData(role);
    case settings::core::SettingId::NetworkPresenceStatusPolicy:
        return presenceStatusDescriptionRoleData(role);
    case settings::core::SettingId::TimelineMediaAutoplayGifVideos:
        return autoplayGifVideosDescriptionRoleData(role);
    case settings::core::SettingId::TimelineMessagesSenderUsername:
        return senderUsernameDescriptionRoleData(role);
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
