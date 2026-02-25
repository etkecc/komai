// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

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

    auto fonts = QFontDatabase::families();
    return applyIndexedOption(v, 0, fonts.size(), [i, fonts](int idx) {
        i->setFontFamily(idx == 0 ? QString{} : fonts.at(idx - 1));
    });
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

    auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
    return applyIndexedOption(v, 0, fonts.size(), [i, fonts](int idx) {
        i->setEmojiFontFamily(idx <= 0 ? QString{} : fonts.at(idx - 1));
    });
}

static QVariant
lookFeelEmojiFontFamilyValues()
{
    auto fonts = QFontDatabase::families(QFontDatabase::WritingSystem::Symbol);
    fonts.prepend(QCoreApplication::translate("UserSettingsModel", "System emoji font"));
    return fonts;
}
