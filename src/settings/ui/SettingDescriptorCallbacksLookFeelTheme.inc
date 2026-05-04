// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

static QVariant
lookFeelThemeValue()
{
    auto i = I;
    if (!i)
        return -1;

    auto variant = ThemeRegistry::instance().themeVariant(i->uiThemeSlug());
    return ThemeRegistry::instance().themeSlugs(variant).indexOf(i->uiThemeSlug());
}

static bool
setLookFeelThemeValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    auto variant = ThemeRegistry::instance().themeVariant(i->uiThemeSlug());
    auto slugs = ThemeRegistry::instance().themeSlugs(variant);
    return applyStringListSelection(v, slugs, [i](const QString &slug) { i->setUiThemeSlug(slug); });
}

static QVariant
lookFeelThemeValues()
{
    auto i = I;
    if (!i)
        return QStringList{};

    return ThemeRegistry::instance().themeNames(ThemeRegistry::instance().themeVariant(i->uiThemeSlug()));
}

// Source-English option lists for cross-locale settings search. Same strings
// as the helpers' translate() calls — lupdate dedupes by (context, source) so
// they share a single translation entry per language.
static const char *const lookFeelScrollbarPolicyValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "When needed"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Never"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Always"),
  nullptr,
};

static QVariant
lookFeelScrollbarPolicyValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "When needed")
           << QCoreApplication::translate("UserSettingsModel", "Never")
           << QCoreApplication::translate("UserSettingsModel", "Always");
    return values;
}

static const char *const lookFeelDefaultAvatarStyleValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Boring Avatars: Bauhaus"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Boring Avatars: Beam"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Boring Avatars: Marble"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Letter initial"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "User icon"),
  nullptr,
};

static QVariant
lookFeelDefaultAvatarStyleValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Boring Avatars: Bauhaus")
           << QCoreApplication::translate("UserSettingsModel", "Boring Avatars: Beam")
           << QCoreApplication::translate("UserSettingsModel", "Boring Avatars: Marble")
           << QCoreApplication::translate("UserSettingsModel", "Letter initial")
           << QCoreApplication::translate("UserSettingsModel", "User icon");
    return values;
}

static const char *const lookFeelDensityValuesEnglish[] = {
  QT_TRANSLATE_NOOP("UserSettingsModel", "Spacious"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Compact"),
  QT_TRANSLATE_NOOP("UserSettingsModel", "Dense"),
  nullptr,
};

static QVariant
lookFeelDensityValues()
{
    QStringList values;
    values << QCoreApplication::translate("UserSettingsModel", "Spacious")
           << QCoreApplication::translate("UserSettingsModel", "Compact")
           << QCoreApplication::translate("UserSettingsModel", "Dense");
    return values;
}
