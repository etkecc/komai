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
lookFeelInputModeValue()
{
    auto i = I;
    if (!i)
        return 0;

    return i->touchInputModeEnabled() ? 1 : 0;
}

static bool
setLookFeelInputModeValue(const QVariant &v)
{
    auto i = I;
    if (!i)
        return false;

    return applyIndexedOption(v, 0, 1, [i](int option) { i->setTouchInputModeEnabled(option == 1); });
}

static QVariant
lookFeelInputModeValues()
{
    return QStringList{
      QCoreApplication::translate("UserSettingsModel", "Desktop"),
      QCoreApplication::translate("UserSettingsModel", "Touch"),
    };
}
