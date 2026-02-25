// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QGuiApplication>
#include <QString>
#include <QStringList>

#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/ThemeRegistry.h"

namespace {

static QStringList
validThemeSlugs()
{
    auto slugs = ThemeRegistry::instance().themeSlugs();
    slugs.append(QStringLiteral("system"));
    return slugs;
}

} // namespace

void
UserSettings::setUiThemeSlug(QString theme)
{
    if (theme == this->uiThemeSlug() || !validThemeSlugs().contains(theme))
        return;
    theme_ = theme;
    (void)coreStore_.set(settings::core::SettingId::UiThemeSlug, theme.toStdString());
    save();
    applyTheme();
    emit uiThemeSlugChanged(theme);
}

int
UserSettings::themeVariantIndex() const
{
    auto variant = ThemeRegistry::instance().themeVariant(uiThemeSlug());
    if (variant == u"light")
        return 0;
    else if (variant == u"dark")
        return 1;
    else
        return 2; // system
}

void
UserSettings::setThemeVariantByIndex(int index)
{
    QString newVariant;
    if (index == 0)
        newVariant = QStringLiteral("light");
    else if (index == 1)
        newVariant = QStringLiteral("dark");
    else
        newVariant = QStringLiteral("system");

    auto currentVariant = ThemeRegistry::instance().themeVariant(uiThemeSlug());
    if (newVariant == currentVariant)
        return;
    setUiThemeSlug(ThemeRegistry::instance().defaultThemeSlug(newVariant));
}

QStringList
UserSettings::themeNamesForCurrentVariant() const
{
    auto variant = ThemeRegistry::instance().themeVariant(uiThemeSlug());
    if (variant == u"system")
        return {};
    return ThemeRegistry::instance().themeNames(variant);
}

int
UserSettings::themeIndexInCurrentVariant() const
{
    auto variant = ThemeRegistry::instance().themeVariant(uiThemeSlug());
    if (variant == u"system")
        return -1;
    auto slugs = ThemeRegistry::instance().themeSlugs(variant);
    return slugs.indexOf(uiThemeSlug());
}

void
UserSettings::setThemeByVariantIndex(int index)
{
    auto variant = ThemeRegistry::instance().themeVariant(uiThemeSlug());
    if (variant == u"system")
        return;
    auto slugs = ThemeRegistry::instance().themeSlugs(variant);
    if (index >= 0 && index < slugs.size())
        setUiThemeSlug(slugs.at(index));
}
