// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <QApplication>
#include <QGuiApplication>
#include <QString>
#include <QStringList>
#include <QStyleHints>

#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/ThemeRegistry.h"

namespace {

static QStringList
validThemeSlugs()
{
    return ThemeRegistry::instance().themeSlugs();
}

} // namespace

bool
UserSettings::applyEffectiveSlug(const QString &slug)
{
    if (slug == this->uiThemeSlug() || !validThemeSlugs().contains(slug))
        return false;
    uiThemeSlug_ = slug;
    (void)coreStore_.set(settings::core::SettingId::UiThemeSlug, slug.toStdString());
    applyTheme();
    emit uiThemeSlugChanged(slug);
    return true;
}

void
UserSettings::setUiThemeSlug(QString theme)
{
    // Persist only on an explicit choice. applyEffectiveSlug never calls save(),
    // so an Auto OS-flip triggers no write of its own. The effective slug can
    // still ride the next explicit save, which is harmless: under Auto the slug
    // only pins the family, and the light/dark half is re-resolved at launch.
    if (applyEffectiveSlug(theme))
        save();
}

int
UserSettings::themeVariantIndex() const
{
    return ThemeRegistry::instance().themeVariant(uiThemeSlug()) == u"dark" ? 1 : 0;
}

QString
UserSettings::counterpartSlugForVariant(const QString &newVariant) const
{
    auto &registry            = ThemeRegistry::instance();
    const auto currentSlug    = uiThemeSlug();
    const auto currentVariant = registry.themeVariant(currentSlug);

    // Prefer the current family's counterpart ("dark-nord" -> "light-nord");
    // if the family has no member in the wanted variant, fall back to that
    // variant's default theme so we still land on the right light/dark surface.
    const auto currentPrefix = currentVariant + QStringLiteral("-");
    if (currentSlug.startsWith(currentPrefix)) {
        const auto candidate =
          newVariant + QStringLiteral("-") + currentSlug.mid(currentPrefix.size());
        const auto *def = registry.findTheme(candidate);
        if (def && def->variant == newVariant)
            return candidate;
    }

    return registry.defaultThemeSlug(newVariant);
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
        return;

    if (newVariant == ThemeRegistry::instance().themeVariant(uiThemeSlug()))
        return;

    setUiThemeSlug(counterpartSlugForVariant(newVariant));
}

void
UserSettings::applyOsColorScheme()
{
    if (uiThemeMode() != ThemeMode::Auto)
        return;

    QString newVariant;
    switch (QGuiApplication::styleHints()->colorScheme()) {
    case Qt::ColorScheme::Light:
        newVariant = QStringLiteral("light");
        break;
    case Qt::ColorScheme::Dark:
        newVariant = QStringLiteral("dark");
        break;
    case Qt::ColorScheme::Unknown:
        // No OS preference (a bare WM, no desktop portal). Leave the current
        // slug alone rather than forcing light.
        return;
    }

    // Runtime-only: repaint to match the OS but never save(), so a nightly
    // light/dark flip can't write config.yml or race a concurrent save.
    applyEffectiveSlug(counterpartSlugForVariant(newVariant));
}

QStringList
UserSettings::themeNamesForCurrentVariant() const
{
    return ThemeRegistry::instance().themeNames(
      ThemeRegistry::instance().themeVariant(uiThemeSlug()));
}

int
UserSettings::themeIndexInCurrentVariant() const
{
    auto variant = ThemeRegistry::instance().themeVariant(uiThemeSlug());
    auto slugs   = ThemeRegistry::instance().themeSlugs(variant);
    return slugs.indexOf(uiThemeSlug());
}

void
UserSettings::setThemeByVariantIndex(int index)
{
    auto variant = ThemeRegistry::instance().themeVariant(uiThemeSlug());
    auto slugs   = ThemeRegistry::instance().themeSlugs(variant);
    if (index >= 0 && index < slugs.size())
        setUiThemeSlug(slugs.at(index));
}
