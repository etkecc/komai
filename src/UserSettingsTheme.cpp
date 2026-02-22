// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QGuiApplication>
#include <QString>
#include <QStringList>

#include "UserSettingsPage.h"
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
UserSettings::setTheme(QString theme)
{
    if (theme == theme_ || !validThemeSlugs().contains(theme))
        return;
    theme_ = theme;
    save();
    applyTheme();
    emit themeChanged(theme);
}

int
UserSettings::themeVariantIndex() const
{
    auto variant = ThemeRegistry::instance().themeVariant(theme());
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

    auto currentVariant = ThemeRegistry::instance().themeVariant(theme());
    if (newVariant == currentVariant)
        return;
    setTheme(ThemeRegistry::instance().defaultThemeSlug(newVariant));
}

QStringList
UserSettings::themeNamesForCurrentVariant() const
{
    auto variant = ThemeRegistry::instance().themeVariant(theme());
    if (variant == u"system")
        return {};
    return ThemeRegistry::instance().themeNames(variant);
}

int
UserSettings::themeIndexInCurrentVariant() const
{
    auto variant = ThemeRegistry::instance().themeVariant(theme());
    if (variant == u"system")
        return -1;
    auto slugs = ThemeRegistry::instance().themeSlugs(variant);
    return slugs.indexOf(theme());
}

void
UserSettings::setThemeByVariantIndex(int index)
{
    auto variant = ThemeRegistry::instance().themeVariant(theme());
    if (variant == u"system")
        return;
    auto slugs = ThemeRegistry::instance().themeSlugs(variant);
    if (index >= 0 && index < slugs.size())
        setTheme(slugs.at(index));
}
